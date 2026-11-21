#ifndef UI_H
#define UI_H

/* Результаты ui_ops.hit(): номер пункта (>= 0) либо одно из значений ниже. */
#define UI_HIT_NONE (-1)
#define UI_HIT_BACK (-2)
#define UI_HIT_HOME (-3)

/* Абстракция вывода. Бэкенды: консольный (ui_console) и фреймбуфер (ui_fb). */
struct ui_ops {
    const char *name;
    int  (*init)(void);
    /* Экран загрузки: сообщение и прогресс 0..100. */
    void (*splash)(const char *msg, int pct);
    /* Меню целиком: заголовок, подписи, выбранный пункт, подсказка. */
    void (*menu)(const char *title, const char *const *labels, int n, int sel,
                 const char *hint);
    /* Окно с текстом (сообщения, лог). */
    void (*lines)(const char *title, const char *const *lines, int n);
    void (*exit)(void);

    /* Необязательные, для тач-управления (у консоли NULL). */
    int  (*size)(int *w, int *h);        /* размер экрана в пикселях */
    void (*touch_mode)(int on);          /* крупные кнопки под палец */
    int  (*hit)(int x, int y);           /* что под пальцем на последнем кадре меню */
};

extern const struct ui_ops ui_console;
extern const struct ui_ops ui_fb;

#endif
