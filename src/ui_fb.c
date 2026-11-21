/* Графический бэкенд на /dev/fb0.
 * Стиль: тёмная основа и нижняя навигационная панель как в TWRP,
 * верхняя панель с маскотом и оранжевым акцентом как в OrangeFox.
 *
 * Для отладки без устройства:
 *   RECOVERY_FB_VIRT=240x240 RECOVERY_FB_DUMP=/tmp/frames ./build/recovery
 * каждый кадр сохраняется в /tmp/frames/frame_NNN.ppm. */

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "font_data.h"
#include "log.h"
#include "mascot.h"
#include "strings.h"
#include "theme.h"
#include "ui.h"

struct font {
    int w, h;
    const unsigned char *d;
};

static struct font F_SMALL, F_BIG;

static int g_w, g_h, g_sc;
static uint32_t *g_bb;

static int g_fd = -1;
static uint8_t *g_map;
static size_t g_maplen;
static int g_bpp, g_stride;
static int g_roff, g_rlen, g_goff, g_glen, g_boff, g_blen;

static const char *g_dumpdir;
static int g_frame;

/* Режим под палец и раскладка последнего меню — для ui_fb.hit(). */
static int g_touch;
static struct {
    int valid;
    int x, w;          /* область кнопок */
    int y0, pitch, ih; /* первая кнопка, шаг, высота */
    int top, shown;    /* первый показанный пункт, сколько показано */
    int nav_y;         /* верх нижней панели */
} g_hit;

/* ---------- примитивы ---------- */

static inline void blend(int x, int y, uint32_t c, int a)
{
    if (x < 0 || y < 0 || x >= g_w || y >= g_h || a <= 0)
        return;
    uint32_t *p = &g_bb[y * g_w + x];
    if (a >= 255) {
        *p = 0xFF000000u | (c & 0xFFFFFFu);
        return;
    }
    uint32_t d = *p;
    int r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
    int g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
    int b = ((c & 255) * a + (d & 255) * (255 - a)) / 255;
    *p = 0xFF000000u | (uint32_t)(r << 16 | g << 8 | b);
}

static void fill_rect(int x, int y, int w, int h, uint32_t c)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            blend(i, j, c, 255);
}

static void fill_rrect(int x, int y, int w, int h, int r, uint32_t c)
{
    if (r * 2 > h)
        r = h / 2;
    if (r * 2 > w)
        r = w / 2;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int dx = i < w - 1 - i ? i : w - 1 - i;
            int dy = j < h - 1 - j ? j : h - 1 - j;
            if (dx < r && dy < r) {
                int ex = r - dx, ey = r - dy;
                if (ex * ex + ey * ey > r * r)
                    continue;
            }
            blend(x + i, y + j, c, 255);
        }
    }
}

static void fill_circle(int cx, int cy, int r, uint32_t c)
{
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r)
                blend(cx + i, cy + j, c, 255);
}

static void gradient_bg(void)
{
    for (int y = 0; y < g_h; y++) {
        int t = y * 255 / (g_h > 1 ? g_h - 1 : 1);
        uint32_t a = COL_BG_TOP, b = COL_BG_BOTTOM, c = 0xFF000000u;
        for (int s = 0; s <= 16; s += 8) {
            int va = (a >> s) & 255, vb = (b >> s) & 255;
            c |= (uint32_t)((va * (255 - t) + vb * t) / 255) << s;
        }
        for (int x = 0; x < g_w; x++)
            g_bb[y * g_w + x] = c;
    }
}

/* ---------- текст ---------- */

static unsigned utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    unsigned c = s[0];

    if (c == 0)
        return 0;
    if (c < 0x80) {
        *ps += 1;
        return c;
    }
    if ((c >> 5) == 6 && s[1]) {
        *ps += 2;
        return ((c & 31u) << 6) | (s[1] & 63u);
    }
    if ((c >> 4) == 14 && s[1] && s[2]) {
        *ps += 3;
        return ((c & 15u) << 12) | ((s[1] & 63u) << 6) | (s[2] & 63u);
    }
    *ps += 1;
    return '?';
}

static int glyph_index(unsigned cp)
{
    for (int i = 0; i < FONT_GLYPHS; i++)
        if (font_codepoints[i] == cp)
            return i;
    return '?' - 0x20;
}

static void draw_glyph(const struct font *f, int x, int y, unsigned cp,
                       uint32_t col)
{
    const unsigned char *g = f->d + (size_t)glyph_index(cp) * f->w * f->h;
    for (int gy = 0; gy < f->h; gy++) {
        for (int gx = 0; gx < f->w; gx++) {
            int a = g[gy * f->w + gx];
            if (!a)
                continue;
            for (int sy = 0; sy < g_sc; sy++)
                for (int sx = 0; sx < g_sc; sx++)
                    blend(x + gx * g_sc + sx, y + gy * g_sc + sy, col, a);
        }
    }
}

static int cp_count(const char *s)
{
    int n = 0;
    while (utf8_next(&s))
        n++;
    return n;
}

static int text_w(const struct font *f, const char *s)
{
    return cp_count(s) * f->w * g_sc;
}

/* Рисует строку, обрезая её многоточием, если не влезает в maxw. */
static int draw_text_fit(const struct font *f, int x, int y, int maxw,
                         const char *s, uint32_t col)
{
    int cw = f->w * g_sc;
    int total = cp_count(s);
    int fit = maxw / cw;
    unsigned cp;
    int i = 0;

    if (fit <= 0)
        return 0;
    if (total <= fit) {
        while ((cp = utf8_next(&s)) != 0) {
            draw_glyph(f, x + i * cw, y, cp, col);
            i++;
        }
        return total * cw;
    }
    while (i < fit - 1 && (cp = utf8_next(&s)) != 0) {
        draw_glyph(f, x + i * cw, y, cp, col);
        i++;
    }
    draw_glyph(f, x + i * cw, y, 0x2026, col);
    return (i + 1) * cw;
}

static void draw_text_center(const struct font *f, int cx, int y, int maxw,
                             const char *s, uint32_t col)
{
    int w = text_w(f, s);
    if (w > maxw)
        w = maxw / (f->w * g_sc) * (f->w * g_sc);
    draw_text_fit(f, cx - w / 2, y, maxw, s, col);
}

/* ---------- маскот и иконки ---------- */

static void draw_mascot(int x, int y, int s)
{
    for (int j = 0; j < MASCOT_H; j++) {
        for (int i = 0; i < MASCOT_W; i++) {
            uint32_t c = mascot_pixel(i, j);
            if (c)
                fill_rect(x + i * s, y + j * s, s, s, c);
        }
    }
}

static void icon_back(int x, int cy, int s, uint32_t col)
{
    for (int dy = -s; dy <= s; dy++) {
        int a = dy < 0 ? -dy : dy;
        fill_rect(x + a, cy + dy, s - a + 1, 1, col);
    }
}

static void icon_home(int cx, int cy, int r, uint32_t col, uint32_t hole)
{
    fill_circle(cx, cy, r, col);
    fill_circle(cx, cy, r - 2 * g_sc > 1 ? r - 2 * g_sc : 1, hole);
}

static int read_battery(void)
{
    const char *e = getenv("RECOVERY_BATTERY");
    if (e)
        return atoi(e);
    FILE *f = fopen("/sys/class/power_supply/battery/capacity", "r");
    int v = -1;
    if (!f)
        return -1;
    if (fscanf(f, "%d", &v) != 1)
        v = -1;
    fclose(f);
    return v;
}

/* ---------- вывод кадра ---------- */

static void present(void)
{
    if (g_dumpdir) {
        char path[300];
        snprintf(path, sizeof(path), "%s/frame_%03d.ppm", g_dumpdir, g_frame++);
        FILE *f = fopen(path, "wb");
        if (!f)
            return;
        fprintf(f, "P6\n%d %d\n255\n", g_w, g_h);
        for (int i = 0; i < g_w * g_h; i++) {
            uint32_t c = g_bb[i];
            fputc((c >> 16) & 255, f);
            fputc((c >> 8) & 255, f);
            fputc(c & 255, f);
        }
        fclose(f);
        return;
    }

    int bytes = g_bpp / 8;
    for (int y = 0; y < g_h; y++) {
        uint8_t *dst = g_map + (size_t)y * g_stride;
        for (int x = 0; x < g_w; x++) {
            uint32_t c = g_bb[y * g_w + x];
            uint32_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            uint32_t v = ((r >> (8 - g_rlen)) << g_roff) |
                         ((g >> (8 - g_glen)) << g_goff) |
                         ((b >> (8 - g_blen)) << g_boff);
            memcpy(dst + x * bytes, &v, bytes);
        }
    }
}

/* ---------- общий каркас экрана ---------- */

struct layout {
    int pad, hdr_h, nav_h, body_y, body_h;
};

static struct layout compute_layout(void)
{
    struct layout L;
    L.pad = 8 * g_sc;
    L.hdr_h = MASCOT_H * g_sc + L.pad + 2 * g_sc; /* вместе с акцентной линией */
    L.nav_h = F_SMALL.h * g_sc + 6 * g_sc;
    L.body_y = L.hdr_h + L.pad / 2;
    L.body_h = g_h - L.body_y - L.nav_h - L.pad / 2;
    return L;
}

static void draw_header(const struct layout *L, const char *title)
{
    int hh = L->hdr_h - 2 * g_sc;
    fill_rect(0, 0, g_w, hh, COL_NAV);
    fill_rect(0, hh, g_w, 2 * g_sc, COL_BAR);
    draw_mascot(L->pad / 2, (hh - MASCOT_H * g_sc) / 2, g_sc);

    int tx = L->pad / 2 + MASCOT_W * g_sc + L->pad / 2;
    int ty = (hh - F_SMALL.h * g_sc) / 2;
    int bat = read_battery();
    char bs[8] = "";
    int bw = 0;

    if (bat >= 0 && bat <= 100) {
        snprintf(bs, sizeof(bs), "%d%%", bat);
        bw = text_w(&F_SMALL, bs);
        draw_text_fit(&F_SMALL, g_w - L->pad / 2 - bw, ty, bw, bs,
                      COL_TEXT_DIM);
        bw += L->pad / 2;
    }
    draw_text_fit(&F_SMALL, tx, ty, g_w - tx - bw - L->pad / 2, title, COL_TEXT);
}

static void draw_nav(const struct layout *L, const char *hint)
{
    int y = g_h - L->nav_h;
    int cy = y + L->nav_h / 2;
    int s = 5 * g_sc;

    fill_rect(0, y, g_w, L->nav_h, COL_NAV);
    icon_back(L->pad, cy, s, COL_TEXT_DIM);
    icon_home(g_w - L->pad - s, cy, s, COL_TEXT_DIM, COL_NAV);
    if (hint) {
        int x0 = L->pad + 2 * s + L->pad / 2;
        int maxw = g_w - 2 * x0;
        draw_text_center(&F_SMALL, g_w / 2, y + (L->nav_h - F_SMALL.h * g_sc) / 2,
                         maxw, hint, COL_TEXT_DIM);
    }
}

/* ---------- ui_ops ---------- */

static void fb_splash(const char *msg, int pct)
{
    int sc = g_sc;
    int ms = (g_w / 2) / MASCOT_W;
    if (ms < 1)
        ms = 1;
    int mw = MASCOT_W * ms, mh = MASCOT_H * ms;
    int my = g_h * 22 / 100;

    gradient_bg();
    fill_circle(g_w / 2, my + mh / 2, mw * 3 / 4, 0xFF1E1830);
    draw_mascot((g_w - mw) / 2, my, ms);

    int y = my + mh + g_h / 20;
    draw_text_center(&F_BIG, g_w / 2, y, g_w - 8 * sc, PRODUCT_NAME, COL_TEXT);
    y += F_BIG.h * sc + 4 * sc;
    draw_text_center(&F_SMALL, g_w / 2, y, g_w - 8 * sc, DEVICE_NAME,
                     COL_TEXT_DIM);

    int bw = g_w * 60 / 100, bh = 6 * sc;
    int bx = (g_w - bw) / 2, by = g_h * 80 / 100;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    fill_rrect(bx, by, bw, bh, bh / 2, COL_CARD);
    int fw = bw * pct / 100;
    if (fw > 0)
        fill_rrect(bx, by, fw < bh ? bh : fw, bh, bh / 2, COL_BAR);
    draw_text_center(&F_SMALL, g_w / 2, by + bh + 6 * sc, g_w - 8 * sc, msg,
                     COL_TEXT_DIM);
    present();
}

static void fb_menu(const char *title, const char *const *labels, int n,
                    int sel, const char *hint)
{
    struct layout L = compute_layout();
    /* под палец кнопки выше, чтобы попадать без стилуса */
    int ih = F_SMALL.h * g_sc + (g_touch ? 14 : 6) * g_sc;
    int gap = 4 * g_sc;
    int pitch = ih + gap;
    int vis = (L.body_h + gap) / pitch;
    if (vis < 1)
        vis = 1;

    int top = 0;
    if (n > vis) {
        top = sel - vis / 2;
        if (top < 0)
            top = 0;
        if (top > n - vis)
            top = n - vis;
    }
    int shown = n < vis ? n : vis;

    gradient_bg();
    draw_header(&L, title);

    int sbw = n > vis ? 4 * g_sc : 0;
    int x = L.pad;
    int w = g_w - 2 * L.pad - (sbw ? sbw + g_sc * 2 : 0);

    g_hit.valid = 1;
    g_hit.x = x;
    g_hit.w = w + (sbw ? sbw + 2 * g_sc : 0); /* полоса прокрутки тоже «пункт» */
    g_hit.y0 = L.body_y;
    g_hit.pitch = pitch;
    g_hit.ih = ih;
    g_hit.top = top;
    g_hit.shown = shown;
    g_hit.nav_y = g_h - L.nav_h;

    for (int k = 0; k < shown; k++) {
        int i = top + k;
        int y = L.body_y + k * pitch;
        int selected = (i == sel);
        fill_rrect(x, y, w, ih, 6 * g_sc, selected ? COL_CARD_SEL : COL_CARD);
        if (!selected) {
            /* тонкая рамка кнопки, как в OrangeFox */
            fill_rrect(x, y, w, ih, 6 * g_sc, COL_CARD_EDGE);
            fill_rrect(x + g_sc, y + g_sc, w - 2 * g_sc, ih - 2 * g_sc,
                       5 * g_sc, COL_CARD);
        }
        draw_text_center(&F_SMALL, x + w / 2, y + (ih - F_SMALL.h * g_sc) / 2,
                         w - 2 * L.pad, labels[i],
                         selected ? COL_TEXT_SEL : COL_TEXT);
    }

    if (sbw) {
        int tx = g_w - L.pad / 2 - sbw;
        int th = shown * pitch - gap;
        fill_rrect(tx, L.body_y, sbw, th, sbw / 2, COL_CARD_EDGE);
        int thumb = th * vis / n;
        if (thumb < 8 * g_sc)
            thumb = 8 * g_sc;
        int ty = L.body_y + (th - thumb) * top / (n - vis);
        fill_rrect(tx, ty, sbw, thumb, sbw / 2, COL_BAR);
    }

    draw_nav(&L, hint);
    present();
}

static void fb_lines(const char *title, const char *const *lines, int n)
{
    struct layout L = compute_layout();
    int cw = F_SMALL.w * g_sc, ch = F_SMALL.h * g_sc;
    int cardx = L.pad, cardw = g_w - 2 * L.pad;
    int cpl = (cardw - L.pad) / cw;
    int rows = (L.body_h - L.pad / 2) / ch;

    gradient_bg();
    draw_header(&L, title);
    fill_rrect(cardx, L.body_y, cardw, L.body_h, 6 * g_sc, COL_LOG_BG);

    /* жёсткий перенос по ширине карточки, затем хвост, чтобы влезло */
    enum { MAXROWS = 128 };
    static char buf[MAXROWS][96];
    int total = 0;
    if (cpl > 95)
        cpl = 95;
    if (cpl < 1)
        cpl = 1;
    for (int i = 0; i < n && total < MAXROWS; i++) {
        const char *s = lines[i];
        int len = cp_count(s);
        if (len == 0) {
            buf[total++][0] = '\0';
            continue;
        }
        while (len > 0 && total < MAXROWS) {
            int take = len < cpl ? len : cpl;
            const char *e = s;
            for (int k = 0; k < take; k++)
                utf8_next(&e);
            size_t bytes = (size_t)(e - s);
            if (bytes >= sizeof(buf[0]))
                bytes = sizeof(buf[0]) - 1;
            memcpy(buf[total], s, bytes);
            buf[total][bytes] = '\0';
            total++;
            s = e;
            len -= take;
        }
    }
    int first = total > rows ? total - rows : 0;
    for (int i = first; i < total; i++)
        draw_text_fit(&F_SMALL, cardx + L.pad / 2,
                      L.body_y + L.pad / 4 + (i - first) * ch,
                      cardw - L.pad, buf[i], COL_LOG_TEXT);

    draw_nav(&L, tr(S_PRESS_ANY));
    present();
}

static int fb_init(void)
{
    const char *virt = getenv("RECOVERY_FB_VIRT");
    g_dumpdir = getenv("RECOVERY_FB_DUMP");

    if (virt) {
        if (sscanf(virt, "%dx%d", &g_w, &g_h) != 2 || g_w < 100 || g_h < 100 ||
            !g_dumpdir) {
            log_msg("fb: bad RECOVERY_FB_VIRT / RECOVERY_FB_DUMP");
            return -1;
        }
    } else {
        static const char *const devs[] = { "/dev/fb0", "/dev/graphics/fb0", NULL };
        for (int i = 0; devs[i] && g_fd < 0; i++)
            g_fd = open(devs[i], O_RDWR | O_CLOEXEC);
        if (g_fd < 0) {
            log_msg("fb: no framebuffer device");
            return -1;
        }
        struct fb_var_screeninfo vi;
        struct fb_fix_screeninfo fi;
        if (ioctl(g_fd, FBIOGET_VSCREENINFO, &vi) < 0 ||
            ioctl(g_fd, FBIOGET_FSCREENINFO, &fi) < 0) {
            log_msg("fb: ioctl failed: %s", strerror(errno));
            return -1;
        }
        vi.yoffset = 0;
        ioctl(g_fd, FBIOPUT_VSCREENINFO, &vi);

        g_w = (int)vi.xres;
        g_h = (int)vi.yres;
        g_bpp = (int)vi.bits_per_pixel;
        g_stride = (int)fi.line_length;
        g_roff = (int)vi.red.offset;
        g_rlen = (int)vi.red.length;
        g_goff = (int)vi.green.offset;
        g_glen = (int)vi.green.length;
        g_boff = (int)vi.blue.offset;
        g_blen = (int)vi.blue.length;
        if (g_bpp != 16 && g_bpp != 24 && g_bpp != 32) {
            log_msg("fb: unsupported bpp %d", g_bpp);
            return -1;
        }
        if (g_rlen == 0 || g_glen == 0 || g_blen == 0 || g_rlen > 8 ||
            g_glen > 8 || g_blen > 8) {
            log_msg("fb: bad channel layout");
            return -1;
        }
        g_maplen = fi.smem_len;
        g_map = mmap(NULL, g_maplen, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
        if (g_map == MAP_FAILED) {
            g_map = NULL;
            log_msg("fb: mmap failed: %s", strerror(errno));
            return -1;
        }
        log_msg("fb: %dx%d %dbpp stride %d", g_w, g_h, g_bpp, g_stride);
    }

    g_bb = calloc((size_t)g_w * g_h, sizeof(uint32_t));
    if (!g_bb)
        return -1;

    int m = g_w < g_h ? g_w : g_h;
    g_sc = m >= 480 ? 2 : 1;

    F_SMALL = (struct font){ FONT_SMALL_W, FONT_SMALL_H, &font_small_data[0][0] };
    F_BIG = (struct font){ FONT_BIG_W, FONT_BIG_H, &font_big_data[0][0] };
    return 0;
}

static int fb_size(int *w, int *h)
{
    if (!g_bb)
        return -1;
    *w = g_w;
    *h = g_h;
    return 0;
}

static void fb_touch_mode(int on)
{
    g_touch = on;
}

/* Что под пальцем на последнем кадре меню. Нижняя панель: левая треть —
 * «назад», правая — «домой». Промежутки между кнопками считаются пустотой. */
static int fb_hit(int x, int y)
{
    if (!g_hit.valid)
        return UI_HIT_NONE;

    if (y >= g_hit.nav_y) {
        if (x < g_w / 3)
            return UI_HIT_BACK;
        if (x >= g_w * 2 / 3)
            return UI_HIT_HOME;
        return UI_HIT_NONE;
    }
    if (x < g_hit.x || x >= g_hit.x + g_hit.w || y < g_hit.y0)
        return UI_HIT_NONE;

    int rel = y - g_hit.y0;
    int k = rel / g_hit.pitch;
    if (k >= g_hit.shown || rel % g_hit.pitch >= g_hit.ih)
        return UI_HIT_NONE;
    return g_hit.top + k;
}

static void fb_exit(void)
{
    if (g_map)
        munmap(g_map, g_maplen);
    if (g_fd >= 0)
        close(g_fd);
    free(g_bb);
    g_map = NULL;
    g_fd = -1;
    g_bb = NULL;
}

const struct ui_ops ui_fb = {
    .name   = "framebuffer",
    .init   = fb_init,
    .splash = fb_splash,
    .menu   = fb_menu,
    .lines  = fb_lines,
    .exit   = fb_exit,
    .size       = fb_size,
    .touch_mode = fb_touch_mode,
    .hit        = fb_hit,
};
