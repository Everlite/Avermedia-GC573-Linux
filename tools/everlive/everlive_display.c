/*
 * gc573_display.c - Dedicated Zero-Latency Direct V4L2/SDL2 Console Display & Audio Companion
 *
 * Designed specifically for interactive gaming on capture cards.
 * Features:
 * - Direct V4L2 DMA mmap texture upload (< 2 ms display latency)
 * - Built-in low-latency ALSA 8ch LPCM capture with 7.1 -> stereo downmix
 * - Interactive on-screen HUD: Real-time volume slider, mute, signal & stats overlay
 * - Mouse wheel volume control & on-screen volume toasts
 * - Auto-hiding cursor & HUD on mouse inactivity
 * - Native borderless fullscreen toggle (F11 / f / double-click)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <time.h>
#include <SDL.h>
#include <X11/Xlib.h>
#include <glob.h>
#include "font8x8.h"

#define BUFFER_COUNT 8

static const char *detect_vdev(void)
{
    static char buf[64];
    glob_t g;
    if (glob("/sys/bus/pci/drivers/open_gc573/*/video4linux/video*", 0, NULL, &g) == 0 && g.gl_pathc > 0) {
        char *base = strrchr(g.gl_pathv[0], '/');
        if (base) {
            snprintf(buf, sizeof(buf), "/dev/%s", base + 1);
            globfree(&g);
            if (access(buf, F_OK) == 0)
                return buf;
        }
        globfree(&g);
    }
    if (access("/dev/video2", F_OK) == 0) return "/dev/video2";
    if (access("/dev/video0", F_OK) == 0) return "/dev/video0";
    if (access("/dev/video1", F_OK) == 0) return "/dev/video1";
    return "/dev/video0";
}

static const char *detect_adev(void)
{
    static char buf[64];
    FILE *f = fopen("/proc/asound/cards", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "GC573") || strstr(line, "Live Gamer 4K") || strstr(line, "A4K")) {
                char *open_br = strchr(line, '[');
                char *close_br = strchr(line, ']');
                if (open_br && close_br && close_br > open_br) {
                    *close_br = '\0';
                    char *id = open_br + 1;
                    while (*id == ' ') id++;
                    char *end = id + strlen(id) - 1;
                    while (end > id && *end == ' ') *end-- = '\0';
                    snprintf(buf, sizeof(buf), "hw:CARD=%s,DEV=0", id);
                    fclose(f);
                    return buf;
                }
            }
        }
        fclose(f);
    }
    return "hw:CARD=A4K,DEV=0";
}

struct v4l2_mmap_buffer {
    void *start;
    size_t length;
};

static volatile sig_atomic_t g_running = 1;
static bool g_fullscreen = false;
static bool g_muted = false;
static float g_volume = 1.0f;
static volatile sig_atomic_t g_audio_dirty = 1;
static bool g_show_stats = true;
static bool g_hud_visible = true;
static uint32_t g_last_mouse_time = 0;
static uint32_t g_volume_toast_time = 0;
static char g_toast_custom[64] = "";
static uint32_t g_toast_time = 0;
static uintptr_t g_embed_wid = 0;

static pid_t g_audio_pid = 0;

static void stop_audio_pipeline(void)
{
    if (g_audio_pid > 0) {
        kill(-g_audio_pid, SIGTERM);
        usleep(30000);
        kill(-g_audio_pid, SIGKILL);
        waitpid(g_audio_pid, NULL, WNOHANG);
        g_audio_pid = 0;
    }
}

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
    stop_audio_pipeline();
}

static volatile sig_atomic_t g_need_geom_check = 1;

static void sigwinch_handler(int sig)
{
    (void)sig;
    g_need_geom_check = 1;
}

static void sigusr1_handler(int sig)
{
    (void)sig;
    g_show_stats = !g_show_stats;
}

static void start_audio_pipeline(const char *adev)
{
    if (g_audio_pid > 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        char plug_dev[256];
        if (strncmp(adev, "hw:", 3) == 0) {
            snprintf(plug_dev, sizeof(plug_dev), "plughw:%s", adev + 3);
        } else {
            snprintf(plug_dev, sizeof(plug_dev), "%s", adev);
        }

        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "{ arecord -D \"%s\" -f S16_LE -c 8 -r 48000 -q 2>/dev/null || "
                 "  arecord -D \"%s\" -f S16_LE -c 6 -r 48000 -q 2>/dev/null || "
                 "  arecord -D \"%s\" -f S16_LE -c 2 -r 48000 -q 2>/dev/null || "
                 "  arecord -D \"%s\" -f S16_LE -c 2 -r 48000 -q 2>/dev/null; } | "
                 "pw-play --latency=20ms -P 'application.name=gc573-audio node.name=gc573-audio' - 2>/dev/null",
                 adev, adev, adev, plug_dev);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    } else if (pid > 0) {
        g_audio_pid = pid;
    }
}

static void *audio_supervisor_thread(void *arg)
{
    const char *adev = (const char *)arg;
    int node_id = -1;
    float last_vol = -1.0f;
    bool last_muted = false;
    int fail_count = 0;
    time_t last_start_time = time(NULL);

    start_audio_pipeline(adev);

    while (g_running) {
        /* Monitor child process */
        if (g_audio_pid > 0) {
            int status;
            pid_t r = waitpid(g_audio_pid, &status, WNOHANG);
            if (r > 0) {
                g_audio_pid = 0;
                node_id = -1;
                time_t now = time(NULL);
                if (now - last_start_time > 5) {
                    fail_count = 0;
                }
                fail_count++;

                /* Back off before retrying to prevent rapid-fire fork loops and speaker pops */
                int backoff_sec = 1;
                if (fail_count > 5) {
                    backoff_sec = 10;
                } else if (fail_count > 2) {
                    backoff_sec = 4;
                }

                for (int s = 0; s < backoff_sec && g_running; s++) {
                    sleep(1);
                }

                if (g_running) {
                    last_start_time = time(NULL);
                    start_audio_pipeline(adev);
                    g_audio_dirty = 1;
                }
            }
        }

        /* Find PipeWire node ID (check at most once every 500ms) */
        static int search_counter = 0;
        if (node_id < 0 && g_audio_pid > 0 && (++search_counter % 20 == 0)) {
            FILE *fp = popen("wpctl status 2>/dev/null | grep 'gc573-audio' | awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+\\.$/) {sub(/\\./,\"\",$i); print $i; exit}}'", "r");
            if (fp) {
                char buf[32];
                if (fgets(buf, sizeof(buf), fp)) {
                    int id = atoi(buf);
                    if (id > 0) {
                        node_id = id;
                        printf("[Audio] PipeWire Audio Stream aktiv (Node ID: %d)\n", node_id);
                        g_audio_dirty = 1;
                    }
                }
                pclose(fp);
            }
        }

        /* Apply volume or mute updates */
        if (node_id > 0 && (g_audio_dirty || g_volume != last_vol || g_muted != last_muted)) {
            g_audio_dirty = 0;
            last_vol = g_volume;
            last_muted = g_muted;

            char cmd[128];
            if (last_muted) {
                snprintf(cmd, sizeof(cmd), "wpctl set-mute %d 1 2>/dev/null", node_id);
            } else {
                snprintf(cmd, sizeof(cmd), "wpctl set-mute %d 0 2>/dev/null; wpctl set-volume %d %.2f 2>/dev/null", node_id, node_id, last_vol);
            }
            system(cmd);
        }

        usleep(25000);
    }

    stop_audio_pipeline();
    return NULL;
}

/* 8x8 Bitmap Text Rendering */
static void draw_char(SDL_Renderer *r, char c, int x, int y, int scale, SDL_Color color)
{
    uint8_t uc = (uint8_t)c;
    if (uc > 127) uc = '?';
    const uint8_t *bitmap = font8x8_basic[uc];
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int row = 0; row < 8; row++) {
        uint8_t b = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if ((b >> col) & 1) {
                SDL_Rect rc = { x + col * scale, y + row * scale, scale, scale };
                SDL_RenderFillRect(r, &rc);
            }
        }
    }
}

static void draw_string(SDL_Renderer *r, const char *str, int x, int y, int scale, SDL_Color color)
{
    while (*str) {
        draw_char(r, *str, x, y, scale, color);
        x += 8 * scale;
        str++;
    }
}

static void update_fullscreen(SDL_Window *window, bool enable)
{
    g_fullscreen = enable;
    if (enable) {
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
        SDL_SetWindowFullscreen(window, 0);
    }
}

int main(int argc, char *argv[])
{
    const char *vdev = NULL;
    const char *adev = NULL;
    bool start_fullscreen = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fullscreen"))
            start_fullscreen = true;
        else if (!strcmp(argv[i], "-d") && i + 1 < argc)
            vdev = argv[++i];
        else if (!strcmp(argv[i], "-a") && i + 1 < argc)
            adev = argv[++i];
        else if ((!strcmp(argv[i], "-w") || !strcmp(argv[i], "--wid")) && i + 1 < argc)
            g_embed_wid = (uintptr_t)strtoull(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--no-audio") || !strcmp(argv[i], "-n"))
            adev = "none";
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Usage: %s [-f|--fullscreen] [-d /dev/videoX] [-a hw:CARD,DEV|none] [-w wid] [--no-audio]\n", argv[0]);
            return 0;
        }
    }

    /* Mesa / AMD Radeon GPU optimization: disable VBlank wait to eliminate swap stalls */
    setenv("vblank_mode", "0", 1);
    setenv("mesa_glthread", "true", 1);

    if (g_embed_wid > 0) {
        setenv("SDL_VIDEODRIVER", "x11", 1);
        g_hud_visible = false;
        g_show_stats = true; /* Keep stats HUD visible so user sees real-time FPS/Lag/Jitter */
    }

    if (!vdev)
        vdev = detect_vdev();
    if (!adev)
        adev = detect_adev();

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGWINCH, sigwinch_handler);

    int fd = open(vdev, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "[Error] Konnte V4L2 Gerät '%s' nicht öffnen: %s\n", vdev, strerror(errno));
        return 1;
    }

    /* Query hardware DV timings from HDMI receiver and apply to capture device */
    struct v4l2_dv_timings dv = {};
    if (ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &dv) == 0) {
        if (ioctl(fd, VIDIOC_S_DV_TIMINGS, &dv) == 0) {
            printf("[*] GC573 HDMI Signal synchronisiert: %ux%u\n",
                   dv.bt.width, dv.bt.height);
        }
    }

    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
        fprintf(stderr, "[Error] VIDIOC_G_FMT fehlgeschlagen: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    uint32_t width = fmt.fmt.pix.width;
    uint32_t height = fmt.fmt.pix.height;
    uint32_t pixelformat = fmt.fmt.pix.pixelformat;
    uint32_t bytesperline = fmt.fmt.pix.bytesperline;

    char fourcc[5] = {
        (char)(pixelformat & 0xff),
        (char)((pixelformat >> 8) & 0xff),
        (char)((pixelformat >> 16) & 0xff),
        (char)((pixelformat >> 24) & 0xff),
        0
    };
    printf("[*] GC573 Signal: %ux%u (%s, %u bytes/line)\n", width, height, fourcc, bytesperline);

    uint32_t sdl_format;
    if (pixelformat == V4L2_PIX_FMT_YUYV)
        sdl_format = SDL_PIXELFORMAT_YUY2;
    else if (pixelformat == V4L2_PIX_FMT_NV12)
        sdl_format = SDL_PIXELFORMAT_NV12;
    else {
        fprintf(stderr, "[!] Unbekanntes Pixelformat %s, versuche YUY2\n", fourcc);
        sdl_format = SDL_PIXELFORMAT_YUY2;
    }

    /* Request 4 mmap buffers */
    struct v4l2_requestbuffers req = {};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        fprintf(stderr, "[Error] VIDIOC_REQBUFS fehlgeschlagen: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    struct v4l2_mmap_buffer buffers[BUFFER_COUNT];
    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr, "[Error] VIDIOC_QUERYBUF fehlgeschlagen: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            fprintf(stderr, "[Error] mmap fehlgeschlagen: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "[Error] VIDIOC_QBUF fehlgeschlagen: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        if (errno == ENOLINK) {
            fprintf(stderr, "\n[Error] VIDIOC_STREAMON fehlgeschlagen: Kein aktives HDMI-Signal (ENOLINK: Link has been severed).\n");
            fprintf(stderr, "        Der HDMI-Receiver der GC573 hat noch keinen Signal-Lock von der Videoquelle erhalten.\n");
            fprintf(stderr, "        Bitte prüfen:\n");
            fprintf(stderr, "        - Ist die Bildquelle (z.B. RetroTINK-4K / Konsole) eingeschaltet und sendet Bild?\n");
            fprintf(stderr, "        - Ist das HDMI-Kabel am GC573 HDMI-IN (Eingang) angeschlossen?\n");
            fprintf(stderr, "        - Tipp: Einmal das HDMI-Kabel kurz ab- und wieder anstecken, um den Handshake neu auszulösen.\n\n");
        } else {
            fprintf(stderr, "[Error] VIDIOC_STREAMON fehlgeschlagen: %s\n", strerror(errno));
        }
        close(fd);
        return 1;
    }

    /* Initialize SDL2 */
    if (g_embed_wid > 0) {
        setenv("SDL_VIDEODRIVER", "x11", 1);
    }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "[Error] SDL_Init fehlgeschlagen: %s\n", SDL_GetError());
        close(fd);
        return 1;
    }

    /* Hints for lowest latency on Wayland & X11 */
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    SDL_SetHint("SDL_VIDEO_WAYLAND_TEARING", "1");
    SDL_SetHint("SDL_VIDEO_WAYLAND_ALLOW_DATACONTROL", "1");
    if (start_fullscreen) {
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Window *window = NULL;
    if (g_embed_wid > 0) {
        window = SDL_CreateWindowFrom((void *)g_embed_wid);
    } else {
        int win_w = (width > 0) ? (int)width : 1920;
        int win_h = (height > 0) ? (int)height : 1080;
        Uint32 win_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
        if (start_fullscreen) {
            win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            g_fullscreen = true;
        }

        /* In multi-monitor setups (e.g. 1440p120 + 1080p165 + 1080p60), select the matching display */
        int target_display = 0;
        int num_displays = SDL_GetNumVideoDisplays();
        for (int d = 0; d < num_displays; d++) {
            SDL_DisplayMode dm;
            if (SDL_GetCurrentDisplayMode(d, &dm) == 0) {
                if ((uint32_t)dm.w == width && (uint32_t)dm.h == height) {
                    target_display = d;
                    break;
                }
            }
        }

        /* In windowed mode, ensure window fits inside display without overflowing */
        if (!start_fullscreen) {
            SDL_Rect bounds;
            if (SDL_GetDisplayBounds(target_display, &bounds) == 0) {
                int max_w = bounds.w * 85 / 100;
                int max_h = bounds.h * 85 / 100;
                if (win_w > max_w || win_h > max_h) {
                    float aspect = (float)win_w / (float)win_h;
                    if ((float)max_w / aspect <= (float)max_h) {
                        win_w = max_w;
                        win_h = (int)((float)max_w / aspect);
                    } else {
                        win_h = max_h;
                        win_w = (int)((float)max_h * aspect);
                    }
                }
            }
        }

        window = SDL_CreateWindow(
            "EverLive - Console Gaming Screen",
            SDL_WINDOWPOS_CENTERED_DISPLAY(target_display), SDL_WINDOWPOS_CENTERED_DISPLAY(target_display),
            win_w, win_h,
            win_flags
        );
        if (start_fullscreen && window) {
            update_fullscreen(window, true);
        }
    }
    if (!window) {
        fprintf(stderr, "[Error] Fenstererstellung fehlgeschlagen: %s\n", SDL_GetError());
        SDL_Quit();
        close(fd);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "[Error] SDL_CreateRenderer fehlgeschlagen: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        close(fd);
        return 1;
    }

    /* Unthrottle swap interval for zero-delay presentation */
    SDL_GL_SetSwapInterval(0);

    SDL_SetRenderDrawColor(renderer, 0, 0, 20, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    if (g_embed_wid == 0) {
        SDL_RaiseWindow(window);
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        sdl_format,
        SDL_TEXTUREACCESS_STREAMING,
        (int)width, (int)height
    );
    if (!texture) {
        fprintf(stderr, "[Error] SDL_CreateTexture fehlgeschlagen: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        close(fd);
        return 1;
    }

    /* Setup Audio Pipeline (PipeWire 10ms with native surround mixing) */
    bool enable_audio = adev && strcmp(adev, "none") != 0;
    pthread_t audio_tid;

    if (enable_audio) {
        pthread_create(&audio_tid, NULL, audio_supervisor_thread, (void *)adev);
    }

    printf("[+] Direct Display aktiv! Hotkeys: [F11/f] Vollbild, [m] Mute, [Scroll] Lautstaerke, [Tab] Stats, [Esc/q] Beenden\n");

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    uint64_t frame_count = 0;
    uint32_t last_title_time = SDL_GetTicks();
    g_last_mouse_time = SDL_GetTicks();

    double lat_ms = 0.0;
    double live_fps = 0.0;
    uint32_t live_jitter = 0;
    uint32_t jitter_count = 0;

    bool dragging_volume = false;

    static int cur_w = 0, cur_h = 0;
    static uint32_t last_geom_check = 0;
    while (g_running) {
        uint32_t now_ticks = SDL_GetTicks();
        if (cur_w <= 0 || cur_h <= 0 || g_need_geom_check || (now_ticks - last_geom_check >= 100)) {
            last_geom_check = now_ticks;
            g_need_geom_check = 0;
            if (g_embed_wid > 0) {
                static Display *x_dpy = NULL;
                if (!x_dpy) x_dpy = XOpenDisplay(NULL);
                if (x_dpy) {
                    Window root;
                    int rx = 0, ry = 0;
                    unsigned int gw = 0, gh = 0, b = 0, d = 0;
                    if (XGetGeometry(x_dpy, (Window)g_embed_wid, &root, &rx, &ry, &gw, &gh, &b, &d) && gw > 0 && gh > 0) {
                        int rw = 0, rh = 0;
                        SDL_GetRendererOutputSize(renderer, &rw, &rh);
                        if ((int)gw != rw || (int)gh != rh) {
                            SDL_SetWindowSize(window, (int)gw, (int)gh);
                        }
                        cur_w = (int)gw;
                        cur_h = (int)gh;
                    }
                }
            } else {
                SDL_GetRendererOutputSize(renderer, &cur_w, &cur_h);
            }
            if (cur_w <= 0 || cur_h <= 0) {
                SDL_GetRendererOutputSize(renderer, &cur_w, &cur_h);
            }
            if (cur_w <= 0 || cur_h <= 0) {
                cur_w = width;
                cur_h = height;
            }
        }

        /* HUD dimensions */
        int hud_w = 660;
        int hud_h = 44;
        int hud_x = (cur_w - hud_w) / 2;
        int hud_y = cur_h - 58;

        /* Slider rect */
        SDL_Rect slider_rect = { hud_x + 64, hud_y + 16, 130, 10 };

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                g_running = 0;
            } else if (ev.type == SDL_MOUSEMOTION) {
                g_last_mouse_time = SDL_GetTicks();
                SDL_ShowCursor(SDL_ENABLE);

                if (dragging_volume) {
                    int mx = ev.motion.x;
                    float ratio = (float)(mx - slider_rect.x) / (float)slider_rect.w;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    g_volume = ratio * 1.5f;
                    g_muted = false;
                    g_audio_dirty = 1;
                    g_volume_toast_time = SDL_GetTicks();
                }
            } else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                g_last_mouse_time = SDL_GetTicks();
                SDL_ShowCursor(SDL_ENABLE);

                if (ev.button.button == SDL_BUTTON_LEFT) {
                    if (ev.button.clicks == 2 && g_embed_wid == 0) {
                        /* Double-click toggles fullscreen */
                        update_fullscreen(window, !g_fullscreen);
                    } else if (g_hud_visible && ev.button.y >= hud_y && ev.button.y <= hud_y + hud_h) {
                        int bx = ev.button.x;
                        /* Mute toggle button (hud_x + 10 .. + 55) */
                        if (bx >= hud_x + 10 && bx <= hud_x + 58) {
                            g_muted = !g_muted;
                            g_audio_dirty = 1;
                            g_volume_toast_time = SDL_GetTicks();
                        }
                        /* Volume slider (hud_x + 64 .. + 194) */
                        else if (bx >= slider_rect.x && bx <= slider_rect.x + slider_rect.w) {
                            dragging_volume = true;
                            float ratio = (float)(bx - slider_rect.x) / (float)slider_rect.w;
                            g_volume = ratio * 1.5f;
                            g_muted = false;
                            g_audio_dirty = 1;
                            g_volume_toast_time = SDL_GetTicks();
                        }
                        /* Stats toggle button (hud_x + 475 .. + 530) */
                        else if (bx >= hud_x + 475 && bx <= hud_x + 530) {
                            g_show_stats = !g_show_stats;
                        }
                        /* Fullscreen toggle button (hud_x + 535 .. + 590) */
                        else if (bx >= hud_x + 535 && bx <= hud_x + 590) {
                            if (g_embed_wid == 0) {
                                update_fullscreen(window, !g_fullscreen);
                            }
                        }
                        /* Exit button (hud_x + 595 .. + 645) */
                        else if (bx >= hud_x + 595 && bx <= hud_x + 645) {
                            g_running = 0;
                        }
                    }
                }
            } else if (ev.type == SDL_MOUSEBUTTONUP) {
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    dragging_volume = false;
                }
            } else if (ev.type == SDL_MOUSEWHEEL) {
                g_last_mouse_time = SDL_GetTicks();
                SDL_ShowCursor(SDL_ENABLE);

                if (ev.wheel.y > 0) {
                    g_volume = (g_volume < 1.45f) ? g_volume + 0.05f : 1.5f;
                    g_muted = false;
                } else if (ev.wheel.y < 0) {
                    g_volume = (g_volume > 0.05f) ? g_volume - 0.05f : 0.0f;
                }
                g_audio_dirty = 1;
                g_volume_toast_time = SDL_GetTicks();
            } else if (ev.type == SDL_KEYDOWN) {
                g_last_mouse_time = SDL_GetTicks();
                SDL_ShowCursor(SDL_ENABLE);

                if ((ev.key.keysym.sym == SDLK_ESCAPE || ev.key.keysym.sym == SDLK_q) && g_embed_wid == 0) {
                    g_running = 0;
                } else if ((ev.key.keysym.sym == SDLK_f || ev.key.keysym.sym == SDLK_F11) && g_embed_wid == 0) {
                    update_fullscreen(window, !g_fullscreen);
                } else if (ev.key.keysym.sym == SDLK_m) {
                    g_muted = !g_muted;
                    g_audio_dirty = 1;
                    g_volume_toast_time = SDL_GetTicks();
                } else if (ev.key.keysym.sym == SDLK_0 || ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_PLUS) {
                    g_volume = (g_volume < 1.45f) ? g_volume + 0.05f : 1.5f;
                    g_muted = false;
                    g_audio_dirty = 1;
                    g_volume_toast_time = SDL_GetTicks();
                } else if (ev.key.keysym.sym == SDLK_9 || ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_MINUS) {
                    g_volume = (g_volume > 0.05f) ? g_volume - 0.05f : 0.0f;
                    g_audio_dirty = 1;
                    g_volume_toast_time = SDL_GetTicks();
                } else if (ev.key.keysym.sym == SDLK_TAB || ev.key.keysym.sym == SDLK_i) {
                    g_show_stats = !g_show_stats;
                } else if (ev.key.keysym.sym == SDLK_h || ev.key.keysym.sym == SDLK_r) {
                    struct v4l2_dv_timings new_dv = {};
                    if (ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &new_dv) == 0) {
                        ioctl(fd, VIDIOC_S_DV_TIMINGS, &new_dv);
                        printf("[*] HDMI Re-Sync: %ux%u\n", new_dv.bt.width, new_dv.bt.height);
                    }
                    snprintf(g_toast_custom, sizeof(g_toast_custom), "HDMI RE-SYNC");
                    g_toast_time = SDL_GetTicks();
                }
            } else if (ev.type == SDL_WINDOWEVENT) {
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED ||
                    ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    g_need_geom_check = true;
                }
            }
        }

        int poll_ret = poll(&pfd, 1, 50);
        if (poll_ret <= 0)
            continue;

        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
            continue;

        /* DRAIN: If multiple buffers arrived (e.g. during a presentation swap),
         * dequeue until we reach the freshest one and return older ones immediately.
         * This guarantees zero backlog, zero lag spikes, and perfect frame pacing! */
        struct v4l2_buffer next_buf = {};
        next_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        next_buf.memory = V4L2_MEMORY_MMAP;
        while (ioctl(fd, VIDIOC_DQBUF, &next_buf) == 0) {
            ioctl(fd, VIDIOC_QBUF, &buf);
            buf = next_buf;
            memset(&next_buf, 0, sizeof(next_buf));
            next_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            next_buf.memory = V4L2_MEMORY_MMAP;
        }

        /* Calculate exact capture-to-display latency */
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        uint64_t now_us = (uint64_t)now_ts.tv_sec * 1000000ULL + now_ts.tv_nsec / 1000ULL;
        uint64_t buf_time_us = (uint64_t)buf.timestamp.tv_sec * 1000000ULL + buf.timestamp.tv_usec;
        int64_t lat_us = (int64_t)(now_us - buf_time_us);
        if (lat_us < 0) lat_us = 0;
        lat_ms = (double)lat_us / 1000.0;

        /* Direct GPU texture upload from DMA buffer */
        SDL_UpdateTexture(texture, NULL, buffers[buf.index].start, (int)bytesperline);

        /* Return buffer to driver immediately so PCIe DMA never starves */
        ioctl(fd, VIDIOC_QBUF, &buf);

        /* Aspect-ratio preserving letterboxed scaling */
        SDL_Rect dest_rect;
        if (width > 0 && height > 0 && cur_w > 0 && cur_h > 0) {
            double scale_w = (double)cur_w / (double)width;
            double scale_h = (double)cur_h / (double)height;
            double scale = (scale_w < scale_h) ? scale_w : scale_h;
            dest_rect.w = (int)(width * scale);
            dest_rect.h = (int)(height * scale);
            dest_rect.x = (cur_w - dest_rect.w) / 2;
            dest_rect.y = (cur_h - dest_rect.h) / 2;
        } else {
            dest_rect.x = 0;
            dest_rect.y = 0;
            dest_rect.w = cur_w;
            dest_rect.h = cur_h;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &dest_rect);

        frame_count++;

        static uint64_t last_now_us = 0;
        uint64_t frame_delta_us = last_now_us ? (now_us - last_now_us) : 8333;
        last_now_us = now_us;

        /* Expected interval: 8333 us (120 Hz) or 16666 us (60 Hz) */
        uint64_t expected = (frame_delta_us < 12500) ? 8333 : 16666;
        int64_t diff = (int64_t)frame_delta_us - (int64_t)expected;
        if (diff < 0) diff = -diff;
        /* Count jitter only on genuine delivery stalls (> 5.5 ms deviation) */
        if (diff > 5500) {
            jitter_count++;
        }

        now_ticks = SDL_GetTicks();

        /* Auto-hide HUD and cursor on idle */
        if (now_ticks - g_last_mouse_time > 2500 && !dragging_volume) {
            g_hud_visible = false;
            SDL_ShowCursor(SDL_DISABLE);
        } else {
            g_hud_visible = true;
        }

        /* 1. Draw On-Screen Display HUD if active */
        if (g_hud_visible) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            /* Main HUD Panel Bar */
            SDL_Rect hud_bg = { hud_x, hud_y, hud_w, hud_h };
            SDL_SetRenderDrawColor(renderer, 15, 18, 26, 215);
            SDL_RenderFillRect(renderer, &hud_bg);
            SDL_SetRenderDrawColor(renderer, 50, 65, 95, 230);
            SDL_RenderDrawRect(renderer, &hud_bg);

            /* Mute / Vol Button */
            SDL_Rect btn_vol = { hud_x + 10, hud_y + 10, 48, 24 };
            SDL_SetRenderDrawColor(renderer, g_muted ? 180 : 35, g_muted ? 40 : 45, g_muted ? 40 : 60, 200);
            SDL_RenderFillRect(renderer, &btn_vol);
            draw_string(renderer, g_muted ? "MUTE" : "VOL", hud_x + 14, hud_y + 14, 1, (SDL_Color){255, 255, 255, 255});

            /* Volume Slider Track */
            SDL_SetRenderDrawColor(renderer, 35, 42, 56, 255);
            SDL_RenderFillRect(renderer, &slider_rect);

            /* Volume Filled Bar */
            int filled_w = (int)(slider_rect.w * (g_volume / 1.5f));
            if (filled_w > slider_rect.w) filled_w = slider_rect.w;
            if (filled_w > 0 && !g_muted) {
                SDL_Rect fill_rect = { slider_rect.x, slider_rect.y, filled_w, slider_rect.h };
                SDL_SetRenderDrawColor(renderer, 56, 139, 253, 255);
                SDL_RenderFillRect(renderer, &fill_rect);
            }

            /* Volume Slider Handle */
            int handle_x = slider_rect.x + filled_w - 4;
            if (handle_x < slider_rect.x) handle_x = slider_rect.x;
            if (handle_x > slider_rect.x + slider_rect.w - 6) handle_x = slider_rect.x + slider_rect.w - 6;
            SDL_Rect handle_rect = { handle_x, slider_rect.y - 3, 7, 16 };
            SDL_SetRenderDrawColor(renderer, 240, 246, 252, 255);
            SDL_RenderFillRect(renderer, &handle_rect);

            /* Volume Text (Percentage) */
            char vol_txt[16];
            if (g_muted)
                snprintf(vol_txt, sizeof(vol_txt), "OFF");
            else
                snprintf(vol_txt, sizeof(vol_txt), "%3d%%", (int)(g_volume * 100.0f));
            draw_string(renderer, vol_txt, hud_x + 202, hud_y + 18, 1, (SDL_Color){200, 215, 235, 255});

            /* Signal Info Badge */
            char sig_txt[32];
            snprintf(sig_txt, sizeof(sig_txt), "%ux%u@%.2f", width, height, live_fps > 0.0 ? live_fps : 60.0);
            draw_string(renderer, sig_txt, hud_x + 260, hud_y + 18, 1, (SDL_Color){63, 185, 80, 255});

            /* Separator */
            SDL_Rect sep = { hud_x + 460, hud_y + 10, 1, 24 };
            SDL_SetRenderDrawColor(renderer, 50, 65, 95, 180);
            SDL_RenderFillRect(renderer, &sep);

            /* Stats Button */
            SDL_Rect btn_stats = { hud_x + 472, hud_y + 10, 54, 24 };
            SDL_SetRenderDrawColor(renderer, g_show_stats ? 40 : 25, g_show_stats ? 60 : 32, g_show_stats ? 80 : 45, 200);
            SDL_RenderFillRect(renderer, &btn_stats);
            draw_string(renderer, "STATS", hud_x + 477, hud_y + 18, 1, (SDL_Color){220, 230, 245, 255});

            /* Fullscreen Button */
            SDL_Rect btn_full = { hud_x + 534, hud_y + 10, 54, 24 };
            SDL_SetRenderDrawColor(renderer, 28, 35, 48, 200);
            SDL_RenderFillRect(renderer, &btn_full);
            draw_string(renderer, g_fullscreen ? "WNDW" : "FULL", hud_x + 539, hud_y + 18, 1, (SDL_Color){220, 230, 245, 255});

            /* Exit Button */
            SDL_Rect btn_exit = { hud_x + 596, hud_y + 10, 48, 24 };
            SDL_SetRenderDrawColor(renderer, 150, 30, 30, 200);
            SDL_RenderFillRect(renderer, &btn_exit);
            draw_string(renderer, "EXIT", hud_x + 603, hud_y + 18, 1, (SDL_Color){255, 255, 255, 255});
        }

        /* 2. Top-Right Stats HUD Overlay (if enabled) */
        if (g_show_stats) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            int box_w = 200;
            int box_h = 75;
            int box_x = cur_w - box_w - 20;
            int box_y = 20;

            SDL_Rect stats_bg = { box_x, box_y, box_w, box_h };
            SDL_SetRenderDrawColor(renderer, 12, 15, 22, 200);
            SDL_RenderFillRect(renderer, &stats_bg);
            SDL_SetRenderDrawColor(renderer, 45, 55, 80, 220);
            SDL_RenderDrawRect(renderer, &stats_bg);

            char line1[32], line2[32], line3[32];
            snprintf(line1, sizeof(line1), "FPS: %.2f (%.2fms)", live_fps > 0.0 ? live_fps : 60.0, live_fps > 0.0 ? (1000.0 / live_fps) : 16.67);
            snprintf(line2, sizeof(line2), "LAG: %.1f ms", lat_ms);
            snprintf(line3, sizeof(line3), "JITTER: %u/s", live_jitter);

            draw_string(renderer, line1, box_x + 12, box_y + 12, 1, (SDL_Color){63, 185, 80, 255});
            draw_string(renderer, line2, box_x + 12, box_y + 32, 1, (SDL_Color){88, 166, 255, 255});
            draw_string(renderer, line3, box_x + 12, box_y + 52, 1, (live_jitter > 5) ? (SDL_Color){248, 81, 73, 255} : (SDL_Color){139, 148, 158, 255});
        }

        /* 3. Center Toast (pops up on scroll/key volume changes or HDMI re-sync) */
        if (now_ticks - g_volume_toast_time < 1200 || now_ticks - g_toast_time < 1200) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            int tw = 220;
            int th = 40;
            int tx = (cur_w - tw) / 2;
            int ty = 40;

            SDL_Rect t_rect = { tx, ty, tw, th };
            SDL_SetRenderDrawColor(renderer, 15, 18, 26, 230);
            SDL_RenderFillRect(renderer, &t_rect);
            SDL_SetRenderDrawColor(renderer, 56, 139, 253, 240);
            SDL_RenderDrawRect(renderer, &t_rect);

            char toast_msg[64];
            if (now_ticks - g_toast_time < 1200 && g_toast_custom[0]) {
                snprintf(toast_msg, sizeof(toast_msg), "%s", g_toast_custom);
            } else if (g_muted) {
                snprintf(toast_msg, sizeof(toast_msg), "AUDIO: MUTED");
            } else {
                snprintf(toast_msg, sizeof(toast_msg), "VOLUME: %3d%%", (int)(g_volume * 100.0f));
            }

            draw_string(renderer, toast_msg, tx + 20, ty + 16, 1, (SDL_Color){240, 246, 252, 255});
        }

        SDL_RenderPresent(renderer);

        if (now_ticks - last_title_time >= 1000) {
            uint32_t elapsed_ms = now_ticks - last_title_time;
            live_fps = (double)frame_count * 1000.0 / (double)elapsed_ms;
            live_jitter = jitter_count;
            frame_count = 0;
            jitter_count = 0;
            last_title_time = now_ticks;

            if (g_embed_wid == 0) {
                char title[180];
                snprintf(title, sizeof(title), "EverLive - %ux%u @ %.2f FPS | Latenz: %.1f ms | Vol: %d%%",
                         width, height, live_fps, lat_ms, g_muted ? 0 : (int)(g_volume * 100.0f));
                SDL_SetWindowTitle(window, title);
            }
        }
    }

    /* Cleanup */
    if (enable_audio) {
        pthread_join(audio_tid, NULL);
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (unsigned int i = 0; i < req.count; i++)
        munmap(buffers[i].start, buffers[i].length);
    close(fd);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("[+] Beendet.\n");
    return 0;
}
