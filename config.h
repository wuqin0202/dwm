/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 10;       /* snap pixel */
static const char *fonts[] = { "Monaco:style=Regular:size=15", "JetBrainsMono Nerd Font:style=Medium:size=15" };
static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const unsigned int alphas[][3]      = {
	/*               fg      bg        border     */
	[SchemeNorm] = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]  = { OPAQUE, baralpha, borderalpha },
};
static const char *colors[][3]      = {
	/*                fg         bg         border   */
	[SchemeNorm] = { "#bbbbbb", "#222222", "#444444"},
	[SchemeSel]  = { "#eeeeee", "#005577", "#005577"},
};

/* tagging */
static const char *tags[] = { "", "", "", "", "", "", "", "﬏", "", "", "ﬄ", "﬐", "", "", ""}; // 最多 31 个

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "floatst",  NULL,       NULL,       0,            1,           -1 },
};

/* layout(s) */
static const float mfact     = 0.62; /* factor of master area size [0.05..0.95] */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function(不能为NULL) */
	{ "﬿",        tile },    /* first entry is default */
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
	{ MODKEY,                       XK_s,      spawn,		   SHCMD("rofi -show drun") },	// 启动程序启动器
	{ MODKEY,             			XK_space,  spawn,          SHCMD("st") },				// 启动终端模拟器
	{ MODKEY,                       XK_b,      togglebar,      {0} },						// 显示隐藏顶部状态栏
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },				// 聚焦下一个窗口
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } }, 				// 聚焦上一个窗口
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },				// 增加 master 窗口数量
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },				// 减少 master 窗口数量
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },				// 减少 master 窗口占比
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} }, 				// 增加 master 窗口占比
	{ MODKEY,                       XK_Return, zoom,           {0} },						// 将聚焦窗口变为 master
	{ MODKEY,                       XK_Tab,    view,           {0} }, 						// 切换历史 tag
	{ MODKEY,             			XK_Escape, killclient,     {0} },						// 关闭窗口
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } }, 				// 光标移动到下一个显示器
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },				// 光标移动到上一个显示器
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } }, 				// 选中所有 tag

	{ MODKEY|ShiftMask,      		XK_space,  spawn,          SHCMD("st -c floatst") },	// 启动终端模拟器
	{ MODKEY|ShiftMask,             XK_Return, togglefloating, {0} },						// 将聚焦窗口变为浮动窗口
	{ MODKEY|ShiftMask,             XK_Escape, quit,           {0} }, 						// 退出 dwm
	{ MODKEY|ShiftMask,             XK_Tab,    setlayout,      {0} }, 						// 切换布局
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } }, 				// 将聚焦窗口移动到下一个显示器
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },				// 将聚焦窗口移动到上一个显示器
	{ MODKEY|ShiftMask,				XK_s,	   spawn,		   SHCMD("flameshot gui") },	// 截屏

	{ 0,          XF86XK_AudioLowerVolume,     spawn,  SHCMD("amixer set Master 5%-") },	// 减小音量
	{ 0,          XF86XK_AudioRaiseVolume,     spawn,  SHCMD("amixer set Master 5%+") },	// 增大音量
	{ 0,          XF86XK_AudioMute,			   spawn,  SHCMD("amixer set Master toggle") },	// 增大音量

	{ ShiftMask,  XF86XK_AudioLowerVolume,     spawn,  SHCMD("xbacklight -dec 5") },	// 减小亮度
	{ 0,		  XF86XK_MonBrightnessDown,    spawn,  SHCMD("xbacklight -dec 5") },	// 减小亮度
	{ ShiftMask,  XF86XK_AudioRaiseVolume,     spawn,  SHCMD("xbacklight -inc 5") },	// 增大亮度
	{ 0,		  XF86XK_MonBrightnessUp,      spawn,  SHCMD("xbacklight -inc 5") },	// 增大亮度

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
};

