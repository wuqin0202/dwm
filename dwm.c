/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask) & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask))
#define INTERSECT(x, y, w, h, m) (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) \
                                * MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))  // 判断窗口是否在选中 tag 上
#define ISOVERVIEW(M) ((M->tagset[M->seltags] == overviewtags))
#define HIDDEN(C) ((getstate(C->win) == IconicState))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)
#define OPAQUE                  0xffU
#define SYSTEM_TRAY_REQUEST_DOCK    0
/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10
#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2
#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeHid, SchemeNormTag, SchemeSelTag, SchemeBarEmpty, SchemeSystray }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMClass, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

typedef struct {
    unsigned int click;
    unsigned int mask;
    unsigned int button;
    void (*func)(const Arg *arg);
    const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
    char name[256];
    float mina, maxa;
    int x, y, w, h;
    int oldx, oldy, oldw, oldh;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
    int bw, oldbw;
    int taskw;  // 在状态栏的宽度
    unsigned int tags;
    int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, isbottom, ishide;
    Client *next;
    Client *snext;
    Monitor *mon;
    Window win;
};

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    const Arg arg;
} Key;

typedef struct {
    const char *symbol;
    void (*arrange)(Monitor *);
} Layout;

struct Monitor {
    char ltsymbol[16];
    float mfact;    // master窗口大小占比
    int nmaster;    // master窗口数量
    int num;        // 编号
    int by;         // bar y
    int mx, my, mw, mh; // monitor，显示器
    int wx, wy, ww, wh; // window，用于放置窗口的区域
    unsigned int bt;      /* number of tasks */
    unsigned int seltags; // 选中的 tag，0 或 1，用于做 tagset 的下标
    unsigned int sellt; // 选中的 layout，0 或 1，用于做 lt 的下标
    unsigned int tagset[2]; // 保存两种 tag 状态
    int showbar; // 是否显示 bar
    int topbar;  // bar 是否在顶部
    Client *clients; // 当前显示器的窗口，为单练表
    Client *sel; // 当前聚焦的窗口
    Client *stack; // 栈区窗口
    Monitor *next; // 下一个显示器
    Window barwin; // bar 窗口，用于显示 bar
    const Layout *lt[2]; // 保存两种布局
};

typedef struct {
    const char *class;
    const char *instance;
    const char *title;
    unsigned int tags;
    int isfloating;
    int isbottom;
    int monitor;
} Rule;

typedef struct {
    Window win;
    Client *icons;
} Systray;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachbottom(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void correct(Monitor *m);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static int drawstatus(Monitor *m);
static void enternotify(XEvent *e);
static void exectagnoc(void);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(int inc, int hid);
static void focusstackhid(const Arg *arg);
static void focusstackvis(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth(void);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void hide(Client *c);
static void hideclient(const Arg *arg);
static void grid(Monitor *m);
static void gridplace(Client *clients, int x, int y, int w, int h, unsigned int gap, Client* (*next)(Client *c));
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nextclient(Client *c);
static Client *nexttiled(Client *c);
static void setfloatingxy(Client *c);
static void pointertoclient(Client *c);
static void pop(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void show(Client *c);
static void showall(Monitor *m);
static void showclient(const Arg *arg);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggleoverview(const Arg *arg);
static void togglesystray(const Arg *arg);
static void toggleview(const Arg *arg);
static void togglewin(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual(void);
static void zoom(const Arg *arg);

/* variables */
static Clr *status_scm;
static Systray *systray = NULL;
static const char broken[] = "broken";
static char stext[1024];
static int screen;
static int sw, sh; /* X display screen geometry width, height */
static int bh;     /* bar height */
static int lrpad;  /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static unsigned int overviewtags;
static void (*handler[LASTEvent])(XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [ResizeRequest] = resizerequest,
    [UnmapNotify] = unmapnotify};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1; // dwm 是否在运行，为 0 退出 dwm
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
    char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
void
applyrules(Client *c)
{
    const char *class, *instance;
    unsigned int i;
    const Rule *r;
    Monitor *m;
    XClassHint ch = { NULL, NULL }; // X 窗口属性，有窗口 name 和 class 两个属性

    /* rule matching */
    c->isfloating = 0;
    c->tags = 0;
    c->isbottom = 0;
    XGetClassHint(dpy, c->win, &ch);
    class = ch.res_class ? ch.res_class : broken;
    instance = ch.res_name ? ch.res_name : broken;
    // 应用 rules
    for (i = 0; i < LENGTH(rules); i++)
    {
        r = &rules[i];
        if ((!r->title || strcmp(c->name, r->title) == 0) &&
            (!r->class || strcmp(class, r->class) == 0) &&
            (!r->instance || strcmp(instance, r->instance) == 0))
        {
            c->isfloating = r->isfloating;
            c->isbottom = r->isbottom;
            c->tags |= r->tags;
            for (m = mons; m && m->num != r->monitor; m = m->next);
            if (m)
                c->mon = m;
        }
    }
    // 释放内存
    if (ch.res_class)
        XFree(ch.res_class);
    if (ch.res_name)
        XFree(ch.res_name);
    // 若未指定 tag 则将窗口放在当先显示器选中的 tag
    if (!(c->tags &= TAGMASK)) // 未指定 tag
    {
        if (ISOVERVIEW(c->mon))
            c->tags = c->mon->tagset[c->mon->seltags ^ 1];
        else
            c->tags = c->mon->tagset[c->mon->seltags];
    }
    c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
    int baseismin;
    Monitor *m = c->mon;

    /* set minimum possible */
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if (interact)
    {
        if (*x > sw)
            *x = sw - WIDTH(c);
        if (*y > sh)
            *y = sh - HEIGHT(c);
        if (*x + *w + 2 * c->bw < 0)
            *x = 0;
        if (*y + *h + 2 * c->bw < 0)
            *y = 0;
    }
    else
    {
        if (*x >= m->wx + m->ww)
            *x = m->wx + m->ww - WIDTH(c);
        if (*y >= m->wy + m->wh)
            *y = m->wy + m->wh - HEIGHT(c);
        if (*x + *w + 2 * c->bw <= m->wx)
            *x = m->wx;
        if (*y + *h + 2 * c->bw <= m->wy)
            *y = m->wy;
    }
    if (*h < bh)
        *h = bh;
    if (*w < bh)
        *w = bh;
    if (c->isfloating)
    {
        if (!c->hintsvalid) // 
            updatesizehints(c);
        /* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = c->basew == c->minw && c->baseh == c->minh;
        if (!baseismin)
        { /* temporarily remove base dimensions */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for aspect limits */
        if (c->mina > 0 && c->maxa > 0)
        {
            if (c->maxa < (float)*w / *h)
                *w = *h * c->maxa + 0.5;
            else if (c->mina < (float)*h / *w)
                *h = *w * c->mina + 0.5;
        }
        if (baseismin)
        { /* increment calculation requires this */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for increment value */
        if (c->incw)
            *w -= *w % c->incw;
        if (c->inch)
            *h -= *h % c->inch;
        /* restore base dimensions */
        *w = MAX(*w + c->basew, c->minw);
        *h = MAX(*h + c->baseh, c->minh);
        if (c->maxw)
            *w = MIN(*w, c->maxw);
        if (c->maxh)
            *h = MIN(*h, c->maxh);
    }
    return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

// 显示显示器 m 选中 tag 的窗口，传递 NULL 为初始化
void
arrange(Monitor *m)
{
    if (m)
        showhide(m->stack);
    else    // 初始化
        for (m = mons; m; m = m->next)
            showhide(m->stack);
    if (m)
    {
        arrangemon(m);
        restack(m);
    }
    else
        for (m = mons; m; m = m->next)
            arrangemon(m);
}

// 当显示器的窗口改变时调用
void
arrangemon(Monitor *m)
{
    strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
    if (ISOVERVIEW(m))
        gridplace(selmon->clients, selmon->wx + gapo, selmon->wy + gapo,
                  selmon->ww - 2 * gapo, selmon->wh - 2 * gapo, gapi, nextclient);
    else
        m->lt[m->sellt]->arrange(m);
}

// 将 c 连接在显示器 clients 的头部
void
attach(Client *c)
{
    c->next = c->mon->clients;
    c->mon->clients = c;
}

void
attachbottom(Client *c)
{
    Client **tc;

    for (tc = &c->mon->clients; *tc; tc = &(*tc)->next);
    *tc = c;
    c->next = NULL;
}

// 将 c 连接在显示器 stack 的头部
void attachstack(Client *c)
{
    c->snext = c->mon->stack;
    c->mon->stack = c;
}

// 处理按键事件
void
buttonpress(XEvent *e)
{
    unsigned int i, x, click;
    unsigned int occ = 0;
    Arg arg = {0};
    Client *c;
    Monitor *m;
    XButtonPressedEvent *ev = &e->xbutton;

    click = ClkRootWin;
    /* focus monitor if necessary */
    if ((m = wintomon(ev->window)) && m != selmon)
    {
        unfocus(selmon->sel, 1);
        selmon = m;
        focus(NULL);
    }
    // 判断是否点击 bar
    if (ev->window == selmon->barwin)
    {
        i = x = 0;
        for(c = m->clients; c; c=c->next)
            occ |= c->tags;
        do {
            /* Do not reserve space for vacant tags */
            if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
                continue;
            x += TEXTW(tags[i]);
        } while (ev->x >= x && ++i < LENGTH(tags));
        if (i < LENGTH(tags))
        {
            click = ClkTagBar;
            arg.ui = 1 << i;
        }
        else if (ev->x < (x = x + TEXTW(selmon->ltsymbol)))
            click = ClkLtSymbol;
        else if (ev->x > selmon->ww - (int)TEXTW(stext) - getsystraywidth())
            click = ClkStatusText;
        else
        {
            if (m->bt == 0)
                return;

            c = m->clients;
            do {
                if (!ISVISIBLE(c))
                    continue;
                else
                    x += c->taskw;
            } while (ev->x > x && (c = c->next));

            if (c) {
                click = ClkWinTitle;
                arg.v = c;
            }
        }
    }
    // 判断是否点击窗口
    else if ((c = wintoclient(ev->window)))
    {
        focus(c);
        restack(selmon);
        XAllowEvents(dpy, ReplayPointer, CurrentTime);
        click = ClkClientWin;
    }
    // 处理调用相应函数
    for (i = 0; i < LENGTH(buttons); i++)
        if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
            buttons[i].func((click == ClkTagBar || click == ClkWinTitle) && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

// 检查是否有其他窗口管理器
void
checkotherwm(void)
{
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

// 释放空间，退出 dwm 时调用
void
cleanup(void)
{
    Monitor *m;
    size_t i;

    for (m = mons; m; m = m->next) // 释放所有显示器的窗口
        while (m->stack)
            unmanage(m->stack, 0);
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    while (mons) // 释放所有 Monitor
        cleanupmon(mons);
    // 释放系统托盘
    if (showsystray) {
        XUnmapWindow(dpy, systray->win);
        XDestroyWindow(dpy, systray->win);
        free(systray);
    }

    for (i = 0; i < CurLast; i++)
        drw_cur_free(drw, cursor[i]);
    for (i = 0; i < LENGTH(colors); i++)
        free(scheme[i]);
    free(scheme);
    XDestroyWindow(dpy, wmcheckwin);
    drw_free(drw);
    XSync(dpy, False);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

// 释放 mon
void
cleanupmon(Monitor *mon)
{
    Monitor *m;

    if (mon == mons)
        mons = mons->next;
    else
    {
        for (m = mons; m && m->next != mon; m = m->next);
        m->next = mon->next;
    }
    XUnmapWindow(dpy, mon->barwin);
    XDestroyWindow(dpy, mon->barwin);
    free(mon);
}

void
clientmessage(XEvent *e)
{
    XWindowAttributes wa;
    XSetWindowAttributes swa;
    XClientMessageEvent *cme = &e->xclient;
    Client *c = wintoclient(cme->window);

    if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP])
    {
        /* add systray icons */
        if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
            if (!(c = (Client *)calloc(1, sizeof(Client))))
                die("fatal: could not malloc() %u bytes\n", sizeof(Client));
            if (!(c->win = cme->data.l[2])) {
                free(c);
                return;
            }
            c->mon = selmon;
            c->next = systray->icons;
            systray->icons = c;
            if (!XGetWindowAttributes(dpy, c->win, &wa)) {
                /* use sane defaults */
                wa.width = bh;
                wa.height = bh;
                wa.border_width = 0;
            }
            c->x = c->oldx = c->y = c->oldy = 0;
            c->w = c->oldw = wa.width;
            c->h = c->oldh = wa.height;
            c->oldbw = wa.border_width;
            c->bw = 0;
            c->isfloating = 1;
            /* reuse tags field as mapped status */
            c->tags = 1;
            updatesizehints(c);
            updatesystrayicongeom(c, wa.width, wa.height);
            XAddToSaveSet(dpy, c->win);
            XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
            XReparentWindow(dpy, c->win, systray->win, 0, 0);
            XClassHint ch = {"dwmsystray", "dwmsystray"};
            XSetClassHint(dpy, c->win, &ch);
            /* use parents background color */
            swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
            XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
            sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            /* FIXME not sure if I have to send these events, too */
            sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
            XSync(dpy, False);
            resizebarwin(selmon);
            updatesystray();
            setclientstate(c, NormalState);
        }
        return;
    }

    if (!c)
        return;
    if (cme->message_type == netatom[NetWMState])
    {
        if (cme->data.l[1] == netatom[NetWMFullscreen]
                || cme->data.l[2] == netatom[NetWMFullscreen])
            setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                              || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
    }
    else if (cme->message_type == netatom[NetActiveWindow])
    {
        if (c != selmon->sel && !c->isurgent)
            seturgent(c, 1);
    }
}

void
configure(Client *c)
{
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->win;
    ce.window = c->win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
    Monitor *m;
    Client *c;
    XConfigureEvent *ev = &e->xconfigure;
    int dirty;

    /* TODO: updategeom handling sucks, needs to be simplified */
    if (ev->window == root)
    {
        dirty = (sw != ev->width || sh != ev->height);
        sw = ev->width;
        sh = ev->height;
        if (updategeom() || dirty)
        {
            drw_resize(drw, sw, bh);
            updatebars();
            for (m = mons; m; m = m->next)
            {
                for (c = m->clients; c; c = c->next)
                    if (c->isfullscreen)
                        resizeclient(c, m->mx, m->my, m->mw, m->mh);
                resizebarwin(m);
            }
            focus(NULL);
            arrange(NULL);
        }
    }
}

void configurerequest(XEvent *e)
{
    Client *c;
    Monitor *m;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    if ((c = wintoclient(ev->window)))
    {
        if (ev->value_mask & CWBorderWidth)
            c->bw = ev->border_width;
        else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange)
        {
            m = c->mon;
            if (ev->value_mask & CWX)
            {
                c->oldx = c->x;
                c->x = m->mx + ev->x;
            }
            if (ev->value_mask & CWY)
            {
                c->oldy = c->y;
                c->y = m->my + ev->y;
            }
            if (ev->value_mask & CWWidth)
            {
                c->oldw = c->w;
                c->w = ev->width;
            }
            if (ev->value_mask & CWHeight)
            {
                c->oldh = c->h;
                c->h = ev->height;
            }
            if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
                c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
            if ((c->y + c->h) > m->my + m->mh && c->isfloating)
                c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
            if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
                configure(c);
            if (ISVISIBLE(c))
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
        }
        else
            configure(c);
    }
    else
    {
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
    XSync(dpy, False);
}

// 更正显示器 m 的窗口状态（应该隐藏但显示的窗口状态更正）
void
correct(Monitor *m)
{
    Client *c;

    for (c = m->clients; c; c = c->next)
    {
        if (c->ishide ^ HIDDEN(c))
            hide(c);
    }
}

Monitor *
createmon(void)
{
    Monitor *m;

    m = ecalloc(1, sizeof(Monitor));
    m->tagset[0] = 1 << defaulttag;
    m->tagset[1] = 1;
    m->mfact = mfact;
    m->nmaster = 1;
    m->showbar = 1;
    m->topbar = 1;
    m->bt = 0;
    m->lt[0] = &layouts[0];
    m->lt[1] = &layouts[1 % LENGTH(layouts)];
    strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
    return m;
}

void destroynotify(XEvent *e)
{
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if ((c = wintoclient(ev->window)))
        unmanage(c, 1);
    else if ((c = wintosystrayicon(ev->window)))
    {
        removesystrayicon(c);
        resizebarwin(selmon);
        updatesystray();
    }
}

void detach(Client *c)
{
    Client **tc;

    for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
        ;
    *tc = c->next;
}

void detachstack(Client *c)
{
    Client **tc, *t;

    for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
        ;
    *tc = c->snext;

    if (c == c->mon->sel)
    {
        for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
            ;
        c->mon->sel = t;
    }
}

Monitor *
dirtomon(int dir)
{
    Monitor *m = NULL;

    if (dir > 0)
    {
        if (!(m = selmon->next))
            m = mons;
    }
    else if (selmon == mons)
        for (m = mons; m->next; m = m->next)
            ;
    else
        for (m = mons; m->next != selmon; m = m->next)
            ;
    return m;
}

void drawbar(Monitor *m)
{
    int x, w, scm, empty_w = m->ww - 2 * barpadh, systray_w = 0, status_w = 0;
    unsigned int i, occ = 0, urg = 0;
    Client *c;

    if (!m->showbar)
        return;
    // 获取系统托盘宽度
    if(showsystray && m == systraytomon(m))
        systray_w = getsystraywidth();
    empty_w -= systray_w;
    // 首先绘制 status 以便后面可被覆盖
    if (m == selmon) // status 只在选中显示器绘制
        status_w = drawstatus(m);
    resizebarwin(m);

    for (c = m->clients; c; c = c->next)
    {
        if (ISVISIBLE(c))
            m->bt++;
        occ |= c->tags;
        if (c->isurgent)
            urg |= c->tags;
    }
    // 绘制 tag
    x = 0;
    if (ISOVERVIEW(m))
    {
        w = TEXTW(overviewsymbol);
        drw_setscheme(drw, scheme[SchemeSel]);
        drw_text(drw, x, 0, w, bh, lrpad / 2, overviewsymbol, 0);
        x += w;
    }
    else
        for (i = 0; i < LENGTH(tags); i++)
        {
            if(!(occ & 1 << i || m->tagset[m->seltags] & 1 << i)) // 跳过空闲的(无窗口) tag
                continue;
            w = TEXTW(tags[i]);
            drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSelTag : SchemeNormTag]);
            drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
            x += w;
        }
    // 绘制 layout
    w = TEXTW(m->ltsymbol);
    drw_setscheme(drw, scheme[SchemeNorm]);
    x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
    // 绘制 title
    empty_w = empty_w - status_w - x;
    for (c = m->clients; c; c = c->next)
    {
        if (!ISVISIBLE(c))
            continue;
        if (m->sel == c)
            scm = SchemeSel;
        else if (c->ishide)
            scm = SchemeHid;
        else
            scm = SchemeNorm;
        drw_setscheme(drw, scheme[scm]);
        w = MIN(TEXTW(c->name), TEXTW("        "));
        if (w > empty_w)
        {
            w = empty_w;
            drw_text(drw, x, 0, w, bh, lrpad / 2, "...", 0);
            c->taskw = w;
            break;
        }
        else
        {
            drw_text(drw, x, 0, w, bh, lrpad / 2, c->name, 0);
            if (c->isfloating)
                drw_rect(drw, x, 0, 7, 7, 0, 0);
            x += w;
            c->taskw = w;
        }
        empty_w -= w; // 剩余宽度
    }
    if (empty_w > 0) // 填充剩余宽度
    {
        drw_setscheme(drw, scheme[SchemeBarEmpty]);
        drw_rect(drw, x, 0, empty_w, bh, 1, 1);
    }
    drw_map(drw, m->barwin, 0, 0, m->ww - systray_w - 2 * barpadh, bh);
}

void drawbars(void)
{
    Monitor *m;

    for (m = mons; m; m = m->next)
        drawbar(m);
}

int
drawstatus(Monitor *m)
{
    int status_w = 0,
        system_w = 0,
        x, w, start, end, count;
    unsigned int alpha;
    char buf8[8] = { [7] = 0 }, buf5[5] = { [4] = 0 },
         text[64];

    if (showsystray && m == systraytomon(m))
        system_w = getsystraywidth();

    // 从后往前绘制
    x = m->ww - system_w - 2 * barpadh;
    end = strlen(stext);
    while (end > 0)
    {
        count = 0;
        for (start = end - 1; start >= 0; start--)
        {
            if (stext[start] == '#')
                count++;
            if (count == 2)
                break;
        }
        if (count == 2)
        {
            strncpy(buf8, stext+start, 7);
            strncpy(buf5, stext+start+7, 4);
            sscanf(buf5, "%x", &alpha);
            drw_clr_create(drw, &status_scm[ColFg], buf8, alpha);
            strncpy(buf8, stext+start+11, 7);
            strncpy(buf5, stext+start+18, 4);
            sscanf(buf5, "%x", &alpha);
            drw_clr_create(drw, &status_scm[ColBg], buf8, alpha);
            strncpy(text, stext+start+22, end-start-22), text[end-start-22] = 0;
            // 开始绘制
            drw_setscheme(drw, status_scm);
            w = TEXTW(text) - lrpad;
            x -= w;
            drw_text(drw, x, 0, w, bh, 0, text, 0);
            status_w += w;
        }
        end = start;
    }
    return status_w;
}

void enternotify(XEvent *e)
{
    Client *c;
    Monitor *m;
    XCrossingEvent *ev = &e->xcrossing;

    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
        return;
    c = wintoclient(ev->window);
    m = c ? c->mon : wintomon(ev->window);
    if (m != selmon)
    {
        unfocus(selmon->sel, 1);
        selmon = m;
    }
    else if (!c || c == selmon->sel)
        return;
    focus(c);
}

void expose(XEvent *e)
{
    Monitor *m;
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0 && (m = wintomon(ev->window)))
    {
        drawbar(m);
        if (m == selmon)
            updatesystray();
    }
}

// 聚焦窗口 c，传递 NULL 自动聚焦
void
focus(Client *c)
{
    if (!c || !ISVISIBLE(c) || HIDDEN(c))
        for (c = selmon->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext);
    if (selmon->sel && selmon->sel != c)
    {
        unfocus(selmon->sel, 0);
        if (selmon->sel->ishide ^ HIDDEN(selmon->sel) && !ISOVERVIEW(selmon))
        {
            hide(selmon->sel);
            if (c)
                arrange(c->mon);
        }
    }
    if (c)
    {
        if (c->mon != selmon)
            selmon = c->mon;
        if (c->isurgent)
            seturgent(c, 0);
        detachstack(c);
        attachstack(c);
        grabbuttons(c, 1);
        XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
        setfocus(c);
    }
    else
    {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
    selmon->sel = c;
    drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e)
{
    XFocusChangeEvent *ev = &e->xfocus;

    if (selmon->sel && ev->window != selmon->sel->win)
        setfocus(selmon->sel);
}

void focusmon(const Arg *arg)
{
    Monitor *m;

    if (!mons->next)
        return;
    if ((m = dirtomon(arg->i)) == selmon)
        return;
    unfocus(selmon->sel, 0);
    selmon = m;
    focus(NULL);
    pointertoclient(selmon->sel);
}

void
focusstackhid(const Arg *arg)
{
    focusstack(arg->i, 1);
}

void
focusstackvis(const Arg *arg)
{
    focusstack(arg->i, 0);
}

void focusstack(int inc, int hid)
{
    Client *c = NULL, *i;
    // 如果窗口全部被隐藏或者选中窗口是全屏则直接返回
    if ((!selmon->sel && !hid) || (selmon->sel && selmon->sel->isfullscreen && lockfullscreen))
        return;
    // 如果选中显示器没开任何窗口则直接返回
    if (!selmon->clients)
        return;
    if (inc > 0)
    {
        if (selmon->sel)
        {
            for (c = selmon->sel->next;
                 c && (!ISVISIBLE(c) || (hid ^ HIDDEN(c)));
                 c = c->next);
        }
        if (!c)
            for (c = selmon->clients;
                 c && (!ISVISIBLE(c) || (hid ^ HIDDEN(c)));
                 c = c->next);
    }
    else
    {
        if (selmon->sel)
        {
            for (i = selmon->clients; i != selmon->sel; i = i->next)
                if (ISVISIBLE(i) && !(hid ^ HIDDEN(i)))
                    c = i;
        }
        else
            c = selmon->clients;
        if (!c)
            for (; i; i = i->next)
                if (ISVISIBLE(i) && !(hid ^ HIDDEN(i)))
                    c = i;
    }
    if (c)
    {
        if (c->ishide)
            show(c);
        focus(c);
        pointertoclient(c);
        arrangemon(selmon);
        restack(selmon);
    }
}

Atom getatomprop(Client *c, Atom prop)
{
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;

    /* FIXME getatomprop should return the number of items and a pointer to
     * the stored data instead of this workaround */
    Atom req = XA_ATOM;
    if (prop == xatom[XembedInfo])
        req = xatom[XembedInfo];

    if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
                           &da, &di, &dl, &dl, &p) == Success &&
        p)
    {
        atom = *(Atom *)p;
        if (da == xatom[XembedInfo] && dl == 2)
            atom = ((Atom *)p)[1];
        XFree(p);
    }
    return atom;
}

int getrootptr(int *x, int *y)
{
    int di;
    unsigned int dui;
    Window dummy;

    return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w)
{
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
                           &real, &format, &n, &extra, (unsigned char **)&p) != Success)
        return -1;
    if (n != 0)
        result = *p;
    XFree(p);
    return result;
}

unsigned int
getsystraywidth(void)
{
    unsigned int w = 0;
    Client *i;
    if(showsystray)
        for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
    return w ? w + systrayspacing + 2 * barpadh : 1;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0)
        return 0;
    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
        return 0;
    if (name.encoding == XA_STRING)
    {
        strncpy(text, (char *)name.value, size - 1);
    }
    else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list)
    {
        strncpy(text, *list, size - 1);
        XFreeStringList(list);
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return 1;
}

void grabbuttons(Client *c, int focused)
{
    updatenumlockmask();
    {
        unsigned int i, j;
        unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        if (!focused)
            XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
                        BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
        for (i = 0; i < LENGTH(buttons); i++)
            if (buttons[i].click == ClkClientWin)
                for (j = 0; j < LENGTH(modifiers); j++)
                    XGrabButton(dpy, buttons[i].button,
                                buttons[i].mask | modifiers[j],
                                c->win, False, BUTTONMASK,
                                GrabModeAsync, GrabModeSync, None, None);
    }
}

void grabkeys(void)
{
    updatenumlockmask();
    {
        unsigned int i, j;
        unsigned int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
        KeyCode code;

        XUngrabKey(dpy, AnyKey, AnyModifier, root);
        for (i = 0; i < LENGTH(keys); i++)
            if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
                for (j = 0; j < LENGTH(modifiers); j++)
                    XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
                             True, GrabModeAsync, GrabModeAsync);
    }
}

// 隐藏窗口但不改变 ishide 值，需自己调用后改变
void
hide(Client *c)
{
    if (!c || HIDDEN(c))
        return;

    Window w = c->win;
    static XWindowAttributes ra, ca;

    // more or less taken directly from blackbox's hide() function
    XGrabServer(dpy);
    XGetWindowAttributes(dpy, root, &ra);
    XGetWindowAttributes(dpy, w, &ca);
    // prevent UnmapNotify events
    XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
    XSelectInput(dpy, w, ca.your_event_mask & ~StructureNotifyMask);
    XUnmapWindow(dpy, w);
    setclientstate(c, IconicState);
    XSelectInput(dpy, root, ra.your_event_mask);
    XSelectInput(dpy, w, ca.your_event_mask);
    XUngrabServer(dpy);
}

void
hideclient(const Arg *arg)
{
    if (!selmon->sel)
        return;
    hide(selmon->sel);
    selmon->sel->ishide = 1;
    focus(NULL);
    arrangemon(selmon);
}

void incnmaster(const Arg *arg)
{
    selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
    arrangemon(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
    while (n--)
        if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org && unique[n].width == info->width && unique[n].height == info->height)
            return 0;
    return 1;
}
#endif /* XINERAMA */

void keypress(XEvent *e)
{
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;

    ev = &e->xkey;
    keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
    for (i = 0; i < LENGTH(keys); i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
            keys[i].func(&(keys[i].arg));
}

void killclient(const Arg *arg)
{
    if (!selmon->sel)
        return;
    if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0))
    {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selmon->sel->win);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
}
// 生成 Client
void
manage(Window w, XWindowAttributes *wa)
{
    Client *c, *t = NULL;
    Window trans = None;
    XWindowChanges wc;

    c = ecalloc(1, sizeof(Client));
    c->win = w;
    c->ishide = 0;
    // 初始化位置大小
    c->x = c->oldx = wa->x;
    c->y = c->oldy = wa->y;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;

    updatetitle(c);
    if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans)))
    {
        c->mon = t->mon;
        c->tags = t->tags;
    }
    else
    {
        c->mon = selmon;
        applyrules(c);
    }

    if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
        c->x = c->mon->wx + c->mon->ww - WIDTH(c);
    if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
        c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
    c->x = MAX(c->x, c->mon->wx);
    c->y = MAX(c->y, c->mon->wy);
    c->bw = borderpx;

    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
    configure(c); /* propagates border_width, if size doesn't change */
    updatewindowtype(c);
    updatesizehints(c);
    updatewmhints(c);
    XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask);
    grabbuttons(c, 0);
    if (!c->isfloating)
        c->isfloating = c->oldstate = trans != None || c->isfixed;
    if (c->isfloating)
    {
        XRaiseWindow(dpy, c->win);
        if (wa->x==0 && wa->y==0)
        {
            c->x = selmon->wx + (selmon->ww - c->w) / 2;
            c->y = selmon->wy + (selmon->wh - c->h) / 2;
        }
        setfloatingxy(c);
    }
    if (c->isbottom)
        attachbottom(c);
    else 
        attach(c);
    attachstack(c);
    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
                    (unsigned char *)&(c->win), 1);
    XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
    setclientstate(c, NormalState);
    if (c->mon == selmon)
        unfocus(selmon->sel, 0);
    c->mon->sel = c;
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    focus(NULL);
}

void mappingnotify(XEvent *e)
{
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard)
        grabkeys();
}

void maprequest(XEvent *e)
{
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    Client *i;
    if ((i = wintosystrayicon(ev->window)))
    {
        sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
        resizebarwin(selmon);
        updatesystray();
    }

    if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
        return;
    if (!wintoclient(ev->window))
        manage(ev->window, &wa);
}

void motionnotify(XEvent *e)
{
    static Monitor *mon = NULL;
    Monitor *m;
    XMotionEvent *ev = &e->xmotion;

    if (ev->window != root)
        return;
    if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon)
    {
        unfocus(selmon->sel, 1);
        selmon = m;
        focus(NULL);
    }
    mon = m;
}

void movemouse(const Arg *arg)
{
    int x, y, ocx, ocy, nx, ny;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel))
        return;
    if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
        return;
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                     None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
        return;
    if (!getrootptr(&x, &y))
        return;
    do
    {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type)
        {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60))
                continue;
            lasttime = ev.xmotion.time;

            nx = ocx + (ev.xmotion.x - x);
            ny = ocy + (ev.xmotion.y - y);
            if (abs(selmon->wx - nx) < snap)
                nx = selmon->wx;
            else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
                nx = selmon->wx + selmon->ww - WIDTH(c);
            if (abs(selmon->wy - ny) < snap)
                ny = selmon->wy;
            else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
                ny = selmon->wy + selmon->wh - HEIGHT(c);
            if (!c->isfloating && selmon->lt[selmon->sellt]->arrange && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
                togglefloating(NULL);
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, nx, ny, c->w, c->h, 1);
            break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon)
    {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

Client
*nextclient(Client *c)
{
    return c;
}

Client *
nexttiled(Client *c)
{
    for (; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next);
    return c;
}

void
setfloatingxy(Client *c)
{
    Client *tc;
    int d1 = 0, d2 = 0, tx, ty;
    int tryed = 0;
    while (tryed++ < 10)
    {
        int dw, dh, existed = 0;
        dw = (selmon->ww / 20) * d1, dh = (selmon->wh / 20) * d2;
        tx = c->x + dw, ty = c->y + dh;
        for (tc = selmon->clients; tc; tc = tc->next)
        {
            if (ISVISIBLE(tc) && !HIDDEN(tc) && tc != c && tc->x == tx && tc->y == ty)
            {
                existed = 1;
                break;
            }
        }
        if (!existed)
        {
            c->x = tx;
            c->y = ty;
            break;
        }
        else
        {
            while (d1 == 0) d1 = rand()%7 - 3;
            while (d2 == 0) d2 = rand()%7 - 3;
        }
    }
}

void
pointertoclient(Client *c)
{
    if (c)
        XWarpPointer(dpy, None, root, 0, 0, 0, 0, c->x + c->w / 2, c->y + c->h / 2);
    else
        XWarpPointer(dpy, None, root, 0, 0, 0, 0, selmon->wx + selmon->ww / 2, selmon->wy + selmon->wh / 2);

}

void pop(Client *c)
{
    detach(c);
    attach(c);
    focus(c);
    arrange(c->mon);
}

void propertynotify(XEvent *e)
{
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((c = wintosystrayicon(ev->window)))
    {
        if (ev->atom == XA_WM_NORMAL_HINTS)
        {
            updatesizehints(c);
            updatesystrayicongeom(c, c->w, c->h);
        }
        else
            updatesystrayiconstate(c, ev);
        resizebarwin(selmon);
        updatesystray();
    }

    if ((ev->window == root) && (ev->atom == XA_WM_NAME))
        updatestatus();
    else if (ev->state == PropertyDelete)
        return; /* ignore */
    else if ((c = wintoclient(ev->window)))
    {
        switch (ev->atom)
        {
        default:
            break;
        case XA_WM_TRANSIENT_FOR:
            if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
                (c->isfloating = (wintoclient(trans)) != NULL))
                arrange(c->mon);
            break;
        case XA_WM_NORMAL_HINTS:
            c->hintsvalid = 0;
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbars();
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName])
        {
            updatetitle(c);
            if (c == c->mon->sel)
                drawbar(c->mon);
        }
        if (ev->atom == netatom[NetWMWindowType])
            updatewindowtype(c);
    }
}

void quit(const Arg *arg)
{
    running = 0;
}

// 
Monitor *
recttomon(int x, int y, int w, int h)
{
    Monitor *m, *r = selmon;
    int a, area = 0;

    for (m = mons; m; m = m->next)
        if ((a = INTERSECT(x, y, w, h, m)) > area)
        {
            area = a;
            r = m;
        }
    return r;
}

void
removesystrayicon(Client *i)
{
    Client **ii;

    if (!showsystray || !i)
        return;
    for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
    if (ii)
        *ii = i->next;
    free(i);
}

void resize(Client *c, int x, int y, int w, int h, int interact)
{
    if (applysizehints(c, &x, &y, &w, &h, interact))
        resizeclient(c, x, y, w, h);
}

void
resizebarwin(Monitor *m) {
    unsigned int w = m->ww;
    if (showsystray && m == systraytomon(m))
        w -= getsystraywidth();
    XMoveResizeWindow(dpy, m->barwin, m->wx + barpadh, m->by, w - 2 * barpadh, bh);
}

void resizeclient(Client *c, int x, int y, int w, int h)
{
    XWindowChanges wc;

    c->oldx = c->x;
    c->x = wc.x = x;
    c->oldy = c->y;
    c->y = wc.y = y;
    c->oldw = c->w;
    c->w = wc.width = w;
    c->oldh = c->h;
    c->h = wc.height = h;
    wc.border_width = c->bw;
    XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

void
resizerequest(XEvent *e)
{
    XResizeRequestEvent *ev = &e->xresizerequest;
    Client *i;

    if ((i = wintosystrayicon(ev->window)))
    {
        updatesystrayicongeom(i, ev->width, ev->height);
        resizebarwin(selmon);
        updatesystray();
    }
}

void resizemouse(const Arg *arg)
{
    int ocx, ocy, nw, nh;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel))
        return;
    if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
        return;
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                     None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
        return;
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    do
    {
        XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
        switch (ev.type)
        {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60))
                continue;
            lasttime = ev.xmotion.time;

            nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
            nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
            if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
            {
                if (!c->isfloating && selmon->lt[selmon->sellt]->arrange && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
                    togglefloating(NULL);
            }
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, c->x, c->y, nw, nh, 1);
            break;
        }
    } while (ev.type != ButtonRelease);
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
        ;
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon)
    {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

void restack(Monitor *m)
{
    Client *c;
    XEvent ev;
    XWindowChanges wc;

    drawbar(m);
    if (!m->sel)
        return;
    if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
        XRaiseWindow(dpy, m->sel->win);
    if (m->lt[m->sellt]->arrange)
    {
        wc.stack_mode = Below;
        wc.sibling = m->barwin;
        for (c = m->stack; c; c = c->snext)
            if (!c->isfloating && ISVISIBLE(c))
            {
                XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
                wc.sibling = c->win;
            }
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
        ;
}

void run(void)
{
    XEvent ev;
    /* main event loop */
    XSync(dpy, False);
    while (running && !XNextEvent(dpy, &ev))
        if (handler[ev.type])
            handler[ev.type](&ev); /* call handler */
}

void scan(void)
{
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num))
    {
        for (i = 0; i < num; i++)
        {
            if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
                manage(wins[i], &wa);
        }
        for (i = 0; i < num; i++)
        { /* now the transients */
            if (!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if (XGetTransientForHint(dpy, wins[i], &d1) && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
                manage(wins[i], &wa);
        }
        if (wins)
            XFree(wins);
    }
}

void sendmon(Client *c, Monitor *m)
{
    if (c->mon == m)
        return;
    unfocus(c, 1);
    detach(c);
    detachstack(c);
    c->mon = m;
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
    if (c->isbottom)
        attachbottom(c);
    else
        attach(c);
    attachstack(c);
    focus(NULL);
    arrange(NULL);
}

void setclientstate(Client *c, long state)
{
    long data[] = {state, None};

    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
                    PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
    int n;
    Atom *protocols, mt;
    int exists = 0;
    XEvent ev;

    if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) 
    {
        mt = wmatom[WMProtocols];
        if (XGetWMProtocols(dpy, w, &protocols, &n))
        {
            while (!exists && n--)
                exists = protocols[n] == proto;
            XFree(protocols);
        }
    }
    else
    {
        exists = True;
        mt = proto;
    }

    if (exists)
    {
        ev.type = ClientMessage;
        ev.xclient.window = w;
        ev.xclient.message_type = mt;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = d0;
        ev.xclient.data.l[1] = d1;
        ev.xclient.data.l[2] = d2;
        ev.xclient.data.l[3] = d3;
        ev.xclient.data.l[4] = d4;
        XSendEvent(dpy, w, False, mask, &ev);
    }
    return exists;
}

void setfocus(Client *c)
{
    if (!c->neverfocus)
    {
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(dpy, root, netatom[NetActiveWindow],
                        XA_WINDOW, 32, PropModeReplace,
                        (unsigned char *)&(c->win), 1);
    }
    sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void setfullscreen(Client *c, int fullscreen)
{
    if (fullscreen && !c->isfullscreen)
    {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen], 1);
        c->isfullscreen = 1;
        c->oldstate = c->isfloating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->isfloating = 1;
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
        XRaiseWindow(dpy, c->win);
    }
    else if (!fullscreen && c->isfullscreen)
    {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)0, 0);
        c->isfullscreen = 0;
        c->isfloating = c->oldstate;
        c->bw = c->oldbw;
        c->x = c->oldx;
        c->y = c->oldy;
        c->w = c->oldw;
        c->h = c->oldh;
        resizeclient(c, c->x, c->y, c->w, c->h);
        arrange(c->mon);
    }
}

void setlayout(const Arg *arg)
{
    if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
        selmon->sellt ^= 1;
    if (arg && arg->v)
        selmon->lt[selmon->sellt] = (Layout *)arg->v;
    strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
    if (selmon->sel)
        arrange(selmon);
    else
        drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg)
{
    float f;

    if (!arg || !selmon->lt[selmon->sellt]->arrange)
        return;
    f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
    if (f < 0.05 || f > 0.95)
        return;
    selmon->mfact = f;
    arrange(selmon);
}

void setup(void)
{
    int i;
    XSetWindowAttributes wa;
    Atom utf8string;

    /* clean up any zombies immediately */
    sigchld(0);

    /* init screen */
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    xinitvisual();
    drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
        die("no fonts could be loaded.");
    lrpad = drw->fonts->h;
    bh = drw->fonts->h + 2;
    updategeom();
    /* init atoms */
    utf8string = XInternAtom(dpy, "UTF8_STRING", False);
    wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
    wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
    wmatom[WMClass] = XInternAtom(dpy, "WM_CLASS", False);
    netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
   netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
    netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
    netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
    netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
    netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
    netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
    xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
    xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
    /* init cursors */
    cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
    cursor[CurResize] = drw_cur_create(drw, XC_sizing);
    cursor[CurMove] = drw_cur_create(drw, XC_fleur);
    /* init appearance */
    scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(colors); i++)
        scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 3);
    status_scm = ecalloc(2, sizeof(XftColor));
    /* init system tray */
    updatesystray();
    /* init bars */
    updatebars();
    updatestatus();
    // 当所有 tag 被选中时触发 overview 状态
    overviewtags = ~0 & TAGMASK;
    /* supporting window for NetWMCheck */
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
                    PropModeReplace, (unsigned char *)"dwm", 3);
    XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&wmcheckwin, 1);
    /* EWMH support per view */
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)netatom, NetLast);
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    /* select events */
    wa.cursor = cursor[CurNormal]->cursor;
    wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    grabkeys();
    focus(NULL);
}

void seturgent(Client *c, int urg)
{
    XWMHints *wmh;

    c->isurgent = urg;
    if (!(wmh = XGetWMHints(dpy, c->win)))
        return;
    wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
    XSetWMHints(dpy, c->win, wmh);
    XFree(wmh);
}

// 显示窗口但不改变 ishide 值，需自己改变
void
show(Client *c)
{
    if (!c || !HIDDEN(c))
        return;

    XMapWindow(dpy, c->win);
    setclientstate(c, NormalState);
}

void
showall(Monitor *m)
{
    Client *c;

    for (c = m->clients; c; c = c->next)
        if (c->ishide)
            show(c);
}

void
showclient(const Arg *arg)
{
    Client *c;

    // 如果当前选中的窗口是隐藏的窗口则将其显示
    if (selmon->sel && selmon->sel->ishide)
        c = selmon->sel;
    else
        for (c = selmon->clients; c && (!ISVISIBLE(c) || !c->ishide); c = c->next);
    if (c)
    {
        show(c);
        c->ishide = 0;
        focus(c);
        arrangemon(selmon);
    }
}

// 显示当前tag下的窗口，切换时会将原窗口下的win放到屏幕之外 (左边的屏幕隐藏到屏幕左边 右边的屏幕隐藏到屏幕右边)
void showhide(Client *c)
{
    if (!c)
        return;
    if (ISVISIBLE(c))
    {
        /* show clients top down */
        XMoveWindow(dpy, c->win, c->x, c->y);
        if (c->isfloating && !c->isfullscreen)
            resize(c, c->x, c->y, c->w, c->h, 0);
        showhide(c->snext);
    }
    else
    {
        /* hide clients bottom up */
        showhide(c->snext);
        if (c->mon->mx == 0)
            XMoveWindow(dpy, c->win, -WIDTH(c), c->y);
        else
            XMoveWindow(dpy, c->win, c->mon->mx + c->mon->mw, c->y);
    }
}

void
togglewin(const Arg *arg)
{
    Client *c = (Client*)arg->v;

    if (c == selmon->sel)
    {
        if (c->ishide)
        {
            show(c);
            c->ishide = 0;
            focus(c);
        }
        else
        {
            hide(c);
            c->ishide = 1;
            focus(NULL);
        }
        arrangemon(c->mon);
    }
    else
    {
        if (c->ishide)
            show(c);
        focus(c);
        arrangemon(selmon);
        restack(selmon);
    }
}

void sigchld(int unused)
{
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        die("can't install SIGCHLD handler:");
    while (0 < waitpid(-1, NULL, WNOHANG))
        ;
}

void spawn(const Arg *arg)
{
    if (fork() == 0)
    {
        if (dpy)
            close(ConnectionNumber(dpy));
        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
    }
}

Monitor *
systraytomon(Monitor *m) {
    Monitor *t;
    int i, n;
    if(!systraypinning) {
        if(!m)
            return selmon;
        return m == selmon ? m : NULL;
    }
    for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
    for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
    if(n < systraypinning)
        return mons;
    return t;
}

void tag(const Arg *arg)
{
    if (selmon->sel && arg->ui & TAGMASK)
    {
        selmon->sel->tags = arg->ui & TAGMASK;
        focus(NULL);
        arrange(selmon);
        view(arg);
    }
}

void tagmon(const Arg *arg)
{
    if (!selmon->sel || !mons->next)
        return;
    sendmon(selmon->sel, dirtomon(arg->i));
    focusmon(arg);
}

void
tile(Monitor *m)
{
    unsigned int i, n, h, r, mw, my, ty;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    if (n == 0) return;

    if (n > m->nmaster)
        mw = m->nmaster ? (m->ww + gapi) * m->mfact : 0;
    else
        mw = m->ww - 2 * gapo + gapi;
    for (i = 0, my = ty = gapo, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
        if (i < m->nmaster)
        {
            r = MIN(n, m->nmaster) - i;
            h = (m->wh - my - gapo - gapi * (r - 1)) / r;
            resize(c,
                   m->wx + gapo,
                   m->wy + my,
                   mw - 2 * c->bw - gapi,
                   h - 2 * c->bw,
                   0);
            my += HEIGHT(c) + gapi;
        }
        else
        {
            r = n - i;
            h = (m->wh - ty - gapo - gapi * (r - 1)) / r;
            resize(c,
                   m->wx + mw + gapo,
                   m->wy + ty,
                   m->ww - mw - 2 * c->bw - 2 * gapo,
                   h - 2* c->bw,
                   0);
            ty += HEIGHT(c) + gapi;
        }
}

void
grid(Monitor *m)
{
    unsigned int n;
    unsigned int cw, ch;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    if (n == 0)
        return;
    else if (n == 1)
    {
        cw = m->ww * 0.7;
        ch = m->wh * 0.65;
        c = nexttiled(m->clients);
        resize(c, m->wx + (m->ww - cw) / 2, m->wy + (m->wh - ch) / 2, cw, ch, 0);
    }
    else if (n == 2)
    {
        cw = (m->ww - gapi - 2 * gapo) / 2;
        ch = m->wh * 0.65;
        c = nexttiled(m->clients);
        resize(c, m->wx + gapo, m->wy + (m->wh - ch) / 2, cw, ch, 0);
        resize(nexttiled(c->next), m->wx + gapo + cw + gapi, m->wy + (m->wh - ch) / 2, cw, ch, 0);
    }
    else
        gridplace(m->clients, m->wx + gapo, m->wy + gapo, m->ww - 2 * gapo, m->wh - 2 * gapo, gapi, nexttiled);
}

void
gridplace(Client *clients, int x, int y, int w, int h, unsigned int gap, Client* (*next)(Client *c))
{
    unsigned int i, j, n;
    unsigned int cx, cy, cw, ch;
    unsigned int cols, rows;
    Client *c;

    for (n = 0, c = next(clients); c; c = next(c->next), n++);
    if (n == 0)
        return;
    getrowcol(n, &rows, &cols);

    ch = (h - (rows - 1) * gap) / rows;
    cw = (w - (cols - 1) * gap) / cols;

    for (i = 0, c = next(clients), cy = y; i < rows - 1; i++)
    {
        cx = x;
        for (j = 0; j < cols; c = next(c->next), j++)
        {
            resize(c, cx, cy, cw - 2 * c->bw, ch - 2 * c->bw, 0);
            cx += cw + gap;
        }
        cy += ch + gap;
    }
    for (cx =  (w - (n - i * cols) * (cw + gap) + gap) / 2 + x; c; c = next(c->next)) 
    {
        resize(c, cx, cy, cw - 2 * c->bw, ch - 2 * c->bw, 0);
        cx += cw + gap;
    }
}

void togglebar(const Arg *arg)
{
    selmon->showbar = !selmon->showbar;
    updatebarpos(selmon);
    resizebarwin(selmon);
    if (showsystray) {
        XWindowChanges wc;
        if (!selmon->showbar)
            wc.y = -bh;
        else if (selmon->showbar) {
            wc.y = 0;
            if (!selmon->topbar)
                wc.y = selmon->mh - bh;
        }
        XConfigureWindow(dpy, systray->win, CWY, &wc);
    }
    arrange(selmon);
}

void togglefloating(const Arg *arg)
{
    if (!selmon->sel)
        return;
    if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
        return;
    selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
    if (selmon->sel->isfloating)
    {
        selmon->sel->x = selmon->wx + selmon->ww / 6,
        selmon->sel->y = selmon->wy + selmon->wh / 6,
        setfloatingxy(selmon->sel);
        resize(selmon->sel, selmon->sel->x, selmon->sel->y,
               selmon->sel->w / 3 * 2, selmon->sel->h / 3 * 2, 0);
    }
    arrange(selmon);
    pointertoclient(selmon->sel);
}

void
toggleoverview(const Arg *arg)
{
    static unsigned int oldtag;

    if (ISOVERVIEW(selmon)) // 正处于 overview 状态，退出
    {
        selmon->tagset[selmon->seltags] = oldtag;
        selmon->seltags ^= 1;
        correct(selmon);
    }
    else
    {
        selmon->seltags ^= 1;
        oldtag = selmon->tagset[selmon->seltags];
        selmon->tagset[selmon->seltags] = overviewtags;
        showall(selmon);
    }
    focus(NULL);
    arrange(selmon);
}

void
togglesystray(const Arg *arg)
{
    if (showsystray)
    {
        showsystray = 0;
        XUnmapWindow(dpy, systray->win);
    }
    else
        showsystray = 1;
    updatesystray();
    updatestatus();
}

void toggleview(const Arg *arg)
{
    if (ISOVERVIEW(selmon))
        return;

    unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

    if (newtagset)
    {
        selmon->tagset[selmon->seltags] = newtagset;
        focus(NULL);
        arrange(selmon);
    }
}

void unfocus(Client *c, int setfocus)
{
    if (!c)
        return;
    grabbuttons(c, 0);
    XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
    if (setfocus)
    {
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
    }
}

// 释放 c
void
unmanage(Client *c, int destroyed)
{
    Monitor *m = c->mon;
    XWindowChanges wc;

    detach(c);
    detachstack(c);
    if (!destroyed)
    {
        wc.border_width = c->oldbw;
        XGrabServer(dpy); /* avoid race conditions */
        XSetErrorHandler(xerrordummy);
        XSelectInput(dpy, c->win, NoEventMask);
        XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        setclientstate(c, WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    free(c);
    focus(NULL);
    updateclientlist();
    arrange(m);
    pointertoclient(selmon->sel);
}

void unmapnotify(XEvent *e)
{
    Client *c;
    XUnmapEvent *ev = &e->xunmap;

    if ((c = wintoclient(ev->window)))
    {
        if (ev->send_event)
            setclientstate(c, WithdrawnState);
        else
            unmanage(c, 0);
    }
    else if ((c = wintosystrayicon(ev->window))) {
        /* KLUDGE! sometimes icons occasionally unmap their windows, but do
         * _not_ destroy them. We map those windows back */
        XMapRaised(dpy, c->win);
        updatesystray();
    }
}

void updatebars(void)
{
    Monitor *m;
    unsigned int w;

    XSetWindowAttributes wa = {
        .override_redirect = True,
        .background_pixmap = ParentRelative,
        .background_pixel = 0,
        .border_pixel = 0,
        .colormap = cmap,
        .event_mask = ButtonPressMask | ExposureMask};
    XClassHint ch = {"dwm", "dwm"};
    for (m = mons; m; m = m->next)
    {
        if (m->barwin)
            continue;
        w = m->ww;
        if (showsystray && m == systraytomon(m))
            w -= getsystraywidth();
        m->barwin = XCreateWindow(dpy, root, m->wx + barpadh, m->by, w - 2 * barpadh, bh, 0, depth,
                                  InputOutput, visual,
                                  CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
        XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
        if (showsystray && m == systraytomon(m))
            XMapRaised(dpy, systray->win);
        XMapRaised(dpy, m->barwin);
        XSetClassHint(dpy, m->barwin, &ch);
    }
}

void updatebarpos(Monitor *m)
{
    m->wy = m->my;
    m->wh = m->mh;
    if (m->showbar)
    {
        m->wh = m->wh - bh - barpadv;
        m->by = m->topbar ? m->wy + barpadv : m->wy + m->wh - barpadv;
        m->wy = m->topbar ? m->wy + bh + barpadv : m->wy;
    }
    else
        m->by = -bh;
}

void updateclientlist()
{
    Client *c;
    Monitor *m;

    XDeleteProperty(dpy, root, netatom[NetClientList]);
    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            XChangeProperty(dpy, root, netatom[NetClientList],
                            XA_WINDOW, 32, PropModeAppend,
                            (unsigned char *)&(c->win), 1);
}

int updategeom(void)
{
    int dirty = 0;

#ifdef XINERAMA
    if (XineramaIsActive(dpy))
    {
        int i, j, n, nn;
        Client *c;
        Monitor *m;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
        XineramaScreenInfo *unique = NULL;

        for (n = 0, m = mons; m; m = m->next, n++)
            ;
        /* only consider unique geometries as separate screens */
        unique = ecalloc(nn, sizeof(XineramaScreenInfo));
        for (i = 0, j = 0; i < nn; i++)
            if (isuniquegeom(unique, j, &info[i]))
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
        XFree(info);
        nn = j;

        /* new monitors if nn > n */
        for (i = n; i < nn; i++)
        {
            for (m = mons; m && m->next; m = m->next)
                ;
            if (m)
                m->next = createmon();
            else
                mons = createmon();
        }
        for (i = 0, m = mons; i < nn && m; m = m->next, i++)
            if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my || unique[i].width != m->mw || unique[i].height != m->mh)
            {
                dirty = 1;
                m->num = i;
                m->mx = m->wx = unique[i].x_org;
                m->my = m->wy = unique[i].y_org;
                m->mw = m->ww = unique[i].width;
                m->mh = m->wh = unique[i].height;
                updatebarpos(m);
            }
        /* removed monitors if n > nn */
        for (i = nn; i < n; i++)
        {
            for (m = mons; m && m->next; m = m->next)
                ;
            while ((c = m->clients))
            {
                dirty = 1;
                m->clients = c->next;
                detachstack(c);
                c->mon = mons;
                if (c->isbottom)
                    attachbottom(c);
                else
                    attach(c);
                attachstack(c);
            }
            if (m == selmon)
                selmon = mons;
            cleanupmon(m);
        }
        free(unique);
    }
    else
#endif /* XINERAMA */
    {  /* default monitor setup */
        if (!mons)
            mons = createmon();
        if (mons->mw != sw || mons->mh != sh)
        {
            dirty = 1;
            mons->mw = mons->ww = sw;
            mons->mh = mons->wh = sh;
            updatebarpos(mons);
        }
    }
    if (dirty)
    {
        selmon = mons;
        selmon = wintomon(root);
    }
    return dirty;
}

void updatenumlockmask(void)
{
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void updatesizehints(Client *c)
{
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
        /* size is uninitialized, ensure that size.flags aren't used */
        size.flags = PSize;
    if (size.flags & PBaseSize)
    {
        c->basew = size.base_width;
        c->baseh = size.base_height;
    }
    else if (size.flags & PMinSize)
    {
        c->basew = size.min_width;
        c->baseh = size.min_height;
    }
    else
        c->basew = c->baseh = 0;
    if (size.flags & PResizeInc)
    {
        c->incw = size.width_inc;
        c->inch = size.height_inc;
    }
    else
        c->incw = c->inch = 0;
    if (size.flags & PMaxSize)
    {
        c->maxw = size.max_width;
        c->maxh = size.max_height;
    }
    else
        c->maxw = c->maxh = 0;
    if (size.flags & PMinSize)
    {
        c->minw = size.min_width;
        c->minh = size.min_height;
    }
    else if (size.flags & PBaseSize)
    {
        c->minw = size.base_width;
        c->minh = size.base_height;
    }
    else
        c->minw = c->minh = 0;
    if (size.flags & PAspect)
    {
        c->mina = (float)size.min_aspect.y / size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    }
    else
        c->maxa = c->mina = 0.0;
    c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
    c->hintsvalid = 1;
}

void updatestatus(void)
{
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
        strcpy(stext, "");
    drawbar(selmon);
    updatesystray();
}


void
updatesystrayicongeom(Client *i, int w, int h)
{
    if (i) {
        i->h = bh;
        if (w == h)
            i->w = bh;
        else if (h == bh)
            i->w = w;
        else
            i->w = (int) ((float)bh * ((float)w / (float)h));
        applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
        /* force icons into the systray dimensions if they don't want to */
        if (i->h > bh) {
            if (i->w == i->h)
                i->w = bh;
            else
                i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
            i->h = bh;
        }
    }
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
    long flags;
    int code = 0;

    if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
            !(flags = getatomprop(i, xatom[XembedInfo])))
        return;

    if (flags & XEMBED_MAPPED && !i->tags) {
        i->tags = 1;
        code = XEMBED_WINDOW_ACTIVATE;
        XMapRaised(dpy, i->win);
        setclientstate(i, NormalState);
    }
    else if (!(flags & XEMBED_MAPPED) && i->tags) {
        i->tags = 0;
        code = XEMBED_WINDOW_DEACTIVATE;
        XUnmapWindow(dpy, i->win);
        setclientstate(i, WithdrawnState);
    }
    else
        return;
    sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
            systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatesystray(void)
{
    XSetWindowAttributes wa;
    XWindowChanges wc;
    Client *i;
    Monitor *m = systraytomon(NULL);
    unsigned int x = m->mx + m->mw;
    unsigned int w = 1;

    if (!showsystray)
        return;
    if (!systray) {
        /* init systray */
        if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
            die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
        systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSystray][ColBg].pixel);
        wa.event_mask        = ButtonPressMask | ExposureMask;
        wa.override_redirect = True;
        wa.background_pixel  = scheme[SchemeSystray][ColBg].pixel;
        XSelectInput(dpy, systray->win, SubstructureNotifyMask);
        XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
                PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
        XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
        XMapRaised(dpy, systray->win);
        XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
        if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
            sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
            XSync(dpy, False);
        }
        else {
            fprintf(stderr, "dwm: unable to obtain system tray.\n");
            free(systray);
            systray = NULL;
            return;
        }
    }
    for (w = 0, i = systray->icons; i; i = i->next) {
        /* make sure the background color stays the same */
        wa.background_pixel  = scheme[SchemeSystray][ColBg].pixel;
        XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
        XMapRaised(dpy, i->win);
        w += systrayspacing;
        i->x = w;
        XMoveResizeWindow(dpy, i->win, i->x + 3, 3, MAX(i->w - 6, bh - 6), bh - 6); // 限制过大图标
        w += MAX(i->w, bh);
        if (i->mon != m)
            i->mon = m;
    }
    w = w ? w + systrayspacing : 1;
    x = x - w - barpadh;
    XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
    wc.x = x; wc.y = m->by; wc.width = w; wc.height = bh;
    wc.stack_mode = Above; wc.sibling = m->barwin;
    XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
    XMapWindow(dpy, systray->win);
    XMapSubwindows(dpy, systray->win);
    XSync(dpy, False);
}

void updatetitle(Client *c)
{
    if (!gettextprop(c->win, wmatom[WMClass], c->name, sizeof c->name))
        gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
    if (c->name[0] == '\0') /* hack to mark broken clients */
        strcpy(c->name, broken);
}

void updatewindowtype(Client *c)
{
    Atom state = getatomprop(c, netatom[NetWMState]);
    Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

    if (state == netatom[NetWMFullscreen])
        setfullscreen(c, 1);
    if (wtype == netatom[NetWMWindowTypeDialog])
        c->isfloating = 1;
}

void updatewmhints(Client *c)
{
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->win)))
    {
        if (c == selmon->sel && wmh->flags & XUrgencyHint)
        {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->win, wmh);
        }
        else
            c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        if (wmh->flags & InputHint)
            c->neverfocus = !wmh->input;
        else
            c->neverfocus = 0;
        XFree(wmh);
    }
}

// 若当前tag无窗口则执行对应的tagcmds 命令
void
exectagnoc(void)
{
    Client *c;
    unsigned int n, i;

    for (n = selmon->tagset[selmon->seltags], i = 0; n; n &= (n - 1), i++); // 统计选中了几个 tag
    if (i > 1)
        return;

    for(n = selmon->tagset[selmon->seltags], i = -1; n; n >>= 1, i++) // 计算当前 tag 对应下标
        ;
    for (n = 0, c = selmon->clients; c; c = c->next) // 统计当前 tag 的窗口数
        if (ISVISIBLE(c))
            n++;
    if (n == 0 && tagcmds[i])
        spawn(&(Arg)SHCMD(tagcmds[i]));
}

void view(const Arg *arg)
{
    if (ISOVERVIEW(selmon))
        return;
    if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    {
        exectagnoc();
        return;
    }
    selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg->ui & TAGMASK)
        selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    exectagnoc();
    focus(NULL);
    arrange(selmon);
}

Client *
wintoclient(Window w)
{
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->win == w)
                return c;
    return NULL;
}

Client *
wintosystrayicon(Window w) {
    Client *i = NULL;

    if (!showsystray || !w)
        return i;
    for (i = systray->icons; i && i->win != w; i = i->next) ;
    return i;
}

// 获取 w 所在的显示器
Monitor *
wintomon(Window w)
{
    int x, y;
    Client *c;
    Monitor *m;

    if (w == root && getrootptr(&x, &y))
        return recttomon(x, y, 1, 1);
    for (m = mons; m; m = m->next)
        if (w == m->barwin)
            return m;
    if ((c = wintoclient(w)))
        return c->mon;
    return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
    if (ee->error_code == BadWindow
            || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
            || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
            || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
            || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
            || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
            || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
            || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
            || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n\r",
            ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); /* may call exit */
}

int xerrordummy(Display *dpy, XErrorEvent *ee)
{
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee)
{
    die("dwm: another window manager is already running");
    return -1;
}

void
xinitvisual(void)
{
    XVisualInfo *infos;
    XRenderPictFormat *fmt;
    int nitems;
    int i;

    XVisualInfo tpl = {
        .screen = screen,
        .depth = 32,
        .class = TrueColor
    };
    long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

    infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
    visual = NULL;
    for(i = 0; i < nitems; i ++) {
        fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
        if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
            visual = infos[i].visual;
            depth = infos[i].depth;
            cmap = XCreateColormap(dpy, root, visual, AllocNone);
            useargb = 1;
            break;
        }
    }

    XFree(infos);

    if (! visual) {
        visual = DefaultVisual(dpy, screen);
        depth = DefaultDepth(dpy, screen);
        cmap = DefaultColormap(dpy, screen);
    }
}

// 将选中窗口置为 master
void
zoom(const Arg *arg)
{
    Client *c = selmon->sel;

    if (!c)
        return;
    if (ISOVERVIEW(selmon))
    {
        toggleoverview(arg);
        selmon->seltags ^= 1;
        selmon->tagset[selmon->seltags] = c->tags;
        if (c->isfloating || c == nexttiled(selmon->clients)) // 浮动窗口或者 c 已经是 master 窗口
        {
            arrange(selmon);
            return;
        }
    }
    if (c->isfloating || c == nexttiled(selmon->clients)) // 浮动窗口或者 c 已经是 master 窗口
        return;
    pop(c);
}

int main(int argc, char *argv[])
{
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("dwm-" VERSION);
    else if (argc != 1)
        die("usage: dwm [-v]");
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
        fputs("warning: no locale support\n", stderr);
    if (!(dpy = XOpenDisplay(NULL)))
        die("dwm: cannot open display");
    checkotherwm();
    system("prime-offload > /var/log/dwm/offload.log");
    setup();
#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", NULL) == -1)
        die("pledge");
#endif /* __OpenBSD__ */
    scan();
    system(autostart);
    run();
    cleanup();
    XCloseDisplay(dpy);
//    system("sudo prime-switch > /var/log/dwm/switch.log");
    return EXIT_SUCCESS;
}
