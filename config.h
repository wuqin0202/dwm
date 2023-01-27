static const char autostart[] = "~/Code/Shell/dwm/autostart.sh &";

/* appearance */
static const int borderpx  = 1;        /* border pixel of windows */
static const int snap = 10;       /* snap pixel */
static const char *fonts[] = { "Monaco:style=Regular:size=15", "JetBrainsMono Nerd Font:style=Medium:pixelsize=20" };
static const int barpadh = 5;
static const int barpadv = 7;
static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const unsigned int alphas[][3] = {
    /*                   fg      bg        border     */
    [SchemeNorm]     = { OPAQUE, baralpha, borderalpha },
    [SchemeSel]      = { OPAQUE, baralpha, borderalpha },
    [SchemeHid]      = { 0x00,   0x00,     0x00 },
    [SchemeNormTag]  = { OPAQUE, baralpha, borderalpha }, 
    [SchemeSelTag]   = { OPAQUE, baralpha, borderalpha },
    [SchemeBarEmpty] = { 0x00,   0x0a,     0x00 },
    [SchemeSystray]  = { OPAQUE, baralpha, borderalpha },
};
static const char *colors[][3] = {
    /*                  fg         bg         border   */
    [SchemeNorm]     = { "#bbbbbb", "#333333", "#444444" },
    [SchemeSel]      = { "#ffffff", "#37474F", "#42A5F5" },
    [SchemeHid]      = { "#dddddd",  NULL,      NULL },
    [SchemeNormTag]  = { "#bbbbbb", "#333333",  NULL },
    [SchemeSelTag]   = { "#eeeeee", "#394857",  NULL },
    [SchemeBarEmpty] = {  NULL,     "#111111",  NULL },
    [SchemeSystray]  = {  NULL,     "#7799AA",  NULL },
};
static const int gapi = 8;             /* 窗口与窗口间隔 */
static const int gapo = 12;            /* 窗口与屏幕边的距离 */
static const int defaulttag = 5;       /* 默认选中的tag的下标 */
static const int systraypinning = 0;   /* 托盘跟随的显示器 0代表不指定显示器 */
static const int systrayspacing = 2;   /* 托盘间距 */
static int showsystray = 1;            /* 是否显示托盘栏 */
static const char *overviewsymbol = "";

/* tagging */
static const char *tags[] = { "", "", "", "", "", "", "", "﬏", "", "", "ﬄ", "﬐", "", "", ""}; // 最多 31 个

/* tagcmds */
static const char *tagcmds[] = { NULL, NULL, NULL, NULL, NULL,\
    "st", "chromium", NULL, "pcmanfm", "wps", "linuxqq", "electronic-wechat-uos-bin", "netease-cloud-music-gtk4", "virt-manager", "obs"};

static const Rule rules[] = {
    /* xprop(1):
     *  WM_CLASS(STRING) = instance, class
     *  WM_NAME(STRING) = title
     */
    /* class               instance    title       tags mask isfloating isbottom monitor */
    { "floatst",           NULL,       NULL,       0,        1,         0,       -1 },
    { "wemeetapp",         NULL,       NULL,       0,        1,         0,       -1 },
    { "st",                NULL,       NULL,       0,        0,         1,       -1 },
    { "chromium",          NULL,       NULL,       1 << 6,   0,         1,       -1 },
    { "qq",                NULL,       NULL,       1 << 10,  0,         1,       -1 },
    { "electronic-wechat", NULL,       NULL,       1 << 11,  0,         1,       -1 },
};

/* layout(s) */
static const float mfact = 0.62; /* factor of master area size [0.05..0.95] */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
    /* symbol     arrange function(不能为NULL) */
    { "﬿",        tile },    /* first entry is default */
    { "﩯",        grid },
};
/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static const Key keys[] = {
    /* modifier                     key        function        argument */
    { MODKEY,                       XK_s,      spawn,          SHCMD("rofi -show drun") },  // 启动程序启动器
    { MODKEY,                       XK_space,  spawn,          SHCMD("st") },               // 启动终端模拟器
    { MODKEY,                       XK_b,      togglebar,      {0} },                       // 显示隐藏顶部状态栏
    { MODKEY,                       XK_j,      focusstackvis,  {.i = +1 } },                // 聚焦下一个可见窗口
    { MODKEY,                       XK_k,      focusstackvis,  {.i = -1 } },                // 聚焦上一个可见窗口
    { MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },                // 增加 master 窗口数量
    { MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },                // 减少 master 窗口数量
    { MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },              // 减少 master 窗口占比
    { MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },              // 增加 master 窗口占比
    { MODKEY,                       XK_Return, zoom,           {0} },                       // 将聚焦窗口变为 master
    { MODKEY,                       XK_Tab,    view,           {0} },                       // 切换历史 tag
    { MODKEY,                       XK_Escape, killclient,     {0} },                       // 关闭窗口
    { MODKEY,                       XK_period, focusmon,       {.i = +1 } },                // 光标移动到下一个显示器
    { MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },                // 光标移动到上一个显示器
    { MODKEY,                       XK_a,      toggleoverview, {0} },                       // 触发 overview 模式
    { MODKEY,              XK_apostrophe,      showclient,     {0} },                       // 显示窗口
    { MODKEY,               XK_semicolon,      hideclient,     {0} },                       // 隐藏窗口

    { MODKEY|ShiftMask,             XK_space,  spawn,          SHCMD("st -c floatst") },    // 启动终端模拟器
    { MODKEY|ShiftMask,             XK_Return, togglefloating, {0} },                       // 将聚焦窗口变为浮动窗口
    { MODKEY|ShiftMask,             XK_Escape, quit,           {0} },                       // 退出 dwm
    { MODKEY|ShiftMask,             XK_Tab,    setlayout,      {0} },                       // 切换布局
    { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },                // 将聚焦窗口移动到下一个显示器
    { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },                // 将聚焦窗口移动到上一个显示器
    { MODKEY|ShiftMask,             XK_s,      spawn,          SHCMD("flameshot gui") },    // 截屏
    { MODKEY|ShiftMask,             XK_j,      focusstackhid,  {.i = +1 } },                // 聚焦下一个隐藏窗口
    { MODKEY|ShiftMask,             XK_k,      focusstackhid,  {.i = -1 } },                // 聚焦上一个隐藏窗口
    { MODKEY|ShiftMask,             XK_b,      togglesystray,  {0} },                       // 显示/隐藏托盘

    { 0,          XF86XK_AudioLowerVolume,     spawn,  SHCMD("amixer set Master 5%-") },    // 减小音量
    { 0,          XF86XK_AudioRaiseVolume,     spawn,  SHCMD("amixer set Master 5%+") },    // 增大音量
    { 0,          XF86XK_AudioMute,            spawn,  SHCMD("amixer set Master toggle") }, // 增大音量

    { ShiftMask,  XF86XK_AudioLowerVolume,     spawn,  SHCMD("xbacklight -dec 5") },    // 减小亮度
    { 0,          XF86XK_MonBrightnessDown,    spawn,  SHCMD("xbacklight -dec 5") },    // 减小亮度
    { ShiftMask,  XF86XK_AudioRaiseVolume,     spawn,  SHCMD("xbacklight -inc 5") },    // 增大亮度
    { 0,          XF86XK_MonBrightnessUp,      spawn,  SHCMD("xbacklight -inc 5") },    // 增大亮度

    TAGKEYS(                        XK_1,                      0)
    TAGKEYS(                        XK_2,                      1)
    TAGKEYS(                        XK_3,                      2)
    TAGKEYS(                        XK_4,                      3)
    TAGKEYS(                        XK_5,                      4)
    TAGKEYS(                        XK_t,                      5)
    TAGKEYS(                        XK_e,                      6)
    TAGKEYS(                        XK_c,                      7)
    TAGKEYS(                        XK_f,                      8)
    TAGKEYS(                        XK_o,                      9)
    TAGKEYS(                        XK_q,                      10)
    TAGKEYS(                        XK_w,                      11)
    TAGKEYS(                        XK_m,                      12)
    TAGKEYS(                        XK_v,                      13)
    TAGKEYS(                        XK_r,                      14)
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
    /* click                event mask      button          function        argument */
    { ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },  // 移动窗口
    { ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },  // 调节窗口大小
    { ClkTagBar,            0,              Button1,        view,           {0} },  // 点击切换tag
    { ClkTagBar,            0,              Button3,        toggleview,     {0} },  // 选中多个tag
    { ClkWinTitle,          0,              Button1,        togglewin,      {0} },  // 聚焦该窗口
};

