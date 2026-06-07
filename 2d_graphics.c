/*
 * ============================================================
 *   2D Text Graphics Editor in C  –  ncurses edition
 *   Canvas  : ROWS x COLS char array drawn with * and _
 *   Shapes  : Circle, Rectangle, Line, Triangle
 *   UI      : ncurses menus, arrow-key navigation, colours
 *
 *   Compile (Linux/Mac):
 *       gcc 2d_graphics_ncurses.c -o gfx -lncurses -lm
 *   Compile (Windows MinGW + PDCurses):
 *       gcc 2d_graphics_ncurses.c -o gfx.exe -lpdcurses -lm
 * ============================================================
 */

#include <pdcurses.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── dimensions ── */
#define ROWS        24
#define COLS        70
#define MAX_OBJECTS 20

/* ── object type codes ── */
#define OBJ_CIRCLE    1
#define OBJ_RECTANGLE 2
#define OBJ_LINE      3
#define OBJ_TRIANGLE  4

/* ── colour pair IDs ── */
#define CP_TITLE   1
#define CP_BORDER  2
#define CP_MENU    3
#define CP_SELECT  4
#define CP_CANVAS  5
#define CP_STATUS  6
#define CP_STAR    7
#define CP_UNDER   8

/* ══════════════════════════════════════════════════════
 *  DATA STRUCTURES
 * ══════════════════════════════════════════════════════ */

typedef struct {
    int  type;
    int  active;
    int  x1, y1, x2, y2, x3, y3;   /* vertices / corners */
    int  r;                           /* radius             */
    char symbol;                      /* '*' or '_'         */
} Object;

static char   canvas[ROWS][COLS];
static Object objects[MAX_OBJECTS];
static int    obj_count = 0;

/* ══════════════════════════════════════════════════════
 *  CANVAS PRIMITIVES
 * ══════════════════════════════════════════════════════ */

static void clear_canvas(void)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            canvas[r][c] = ' ';
}

static void set_pixel(int r, int c, char ch)
{
    if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
        canvas[r][c] = ch;
}

/* ── Bresenham midpoint circle ── */
static void draw_circle(int cx, int cy, int radius, char ch)
{
    int x = 0, y = radius, d = 1 - radius;
    while (x <= y) {
        set_pixel(cy+y, cx+2*x, ch); set_pixel(cy+y, cx-2*x, ch);
        set_pixel(cy-y, cx+2*x, ch); set_pixel(cy-y, cx-2*x, ch);
        set_pixel(cy+x, cx+2*y, ch); set_pixel(cy+x, cx-2*y, ch);
        set_pixel(cy-x, cx+2*y, ch); set_pixel(cy-x, cx-2*y, ch);
        if (d < 0) d += 2*x+3;
        else { d += 2*(x-y)+5; y--; }
        x++;
    }
}

/* ── Bresenham line ── */
static void draw_line(int r1,int c1,int r2,int c2,char ch)
{
    int dr=abs(r2-r1), dc=abs(c2-c1);
    int sr=(r1<r2)?1:-1, sc=(c1<c2)?1:-1;
    int err=dr-dc, e2;
    for(;;){
        set_pixel(r1,c1,ch);
        if(r1==r2&&c1==c2) break;
        e2=2*err;
        if(e2>-dc){err-=dc; r1+=sr;}
        if(e2< dr){err+=dr; c1+=sc;}
    }
}

/* ── Hollow rectangle ── */
static void draw_rectangle(int r1,int c1,int r2,int c2,char ch)
{
    if(r1>r2){int t=r1;r1=r2;r2=t;}
    if(c1>c2){int t=c1;c1=c2;c2=t;}
    for(int c=c1;c<=c2;c++){set_pixel(r1,c,ch);set_pixel(r2,c,ch);}
    for(int r=r1;r<=r2;r++){set_pixel(r,c1,ch);set_pixel(r,c2,ch);}
}

/* ── Triangle from 3 vertices ── */
static void draw_triangle(int r1,int c1,int r2,int c2,int r3,int c3,char ch)
{
    draw_line(r1,c1,r2,c2,ch);
    draw_line(r2,c2,r3,c3,ch);
    draw_line(r3,c3,r1,c1,ch);
}

/* ── Redraw canvas from object list ── */
static void redraw_all(void)
{
    clear_canvas();
    for(int i=0;i<obj_count;i++){
        if(!objects[i].active) continue;
        Object *o=&objects[i];
        switch(o->type){
            case OBJ_CIRCLE:    draw_circle(o->x1,o->y1,o->r,o->symbol);                             break;
            case OBJ_RECTANGLE: draw_rectangle(o->y1,o->x1,o->y2,o->x2,o->symbol);                  break;
            case OBJ_LINE:      draw_line(o->y1,o->x1,o->y2,o->x2,o->symbol);                        break;
            case OBJ_TRIANGLE:  draw_triangle(o->y1,o->x1,o->y2,o->x2,o->y3,o->x3,o->symbol);       break;
        }
    }
}

/* ══════════════════════════════════════════════════════
 *  NCURSES HELPERS
 * ══════════════════════════════════════════════════════ */

/* Draw a titled box on window w */
static void draw_box_title(WINDOW *w, const char *title)
{
    box(w, 0, 0);
    int wide = getmaxx(w);
    wattron(w, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(w, 0, (wide - (int)strlen(title) - 2) / 2, " %s ", title);
    wattroff(w, COLOR_PAIR(CP_TITLE) | A_BOLD);
}

/* Status bar at bottom of stdscr */
static void show_status(const char *msg)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    mvhline(rows-1, 0, ' ', cols);
    mvprintw(rows-1, 1, "%s", msg);
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
    refresh();
}

/* Read an integer with a prompt inside window w at given line */
static int win_get_int(WINDOW *w, int line, const char *prompt)
{
    mvwprintw(w, line, 2, "%-36s", prompt);
    wrefresh(w);
    echo();
    curs_set(1);
    int v = 0;
    char buf[16];
    mvwgetnstr(w, line, 2+(int)strlen(prompt), buf, 10);
    v = atoi(buf);
    noecho();
    curs_set(0);
    return v;
}

/* Read a char with a prompt inside window w */
static char win_get_sym(WINDOW *w, int line)
{
    mvwprintw(w, line, 2, "Symbol (* or _): ");
    wrefresh(w);
    echo();
    curs_set(1);
    char buf[4];
    mvwgetnstr(w, line, 19, buf, 2);
    noecho();
    curs_set(0);
    char sym = buf[0];
    if(sym!='*'&&sym!='_') sym='*';
    return sym;
}

/* ══════════════════════════════════════════════════════
 *  CANVAS DISPLAY WINDOW
 * ══════════════════════════════════════════════════════ */

static void show_canvas_window(void)
{
    /* Canvas window: ROWS+2 tall, COLS+2 wide, centred */
    int scr_rows, scr_cols;
    getmaxyx(stdscr, scr_rows, scr_cols);

    int win_h = ROWS + 2;
    int win_w = COLS + 2;
    int start_y = (scr_rows - win_h) / 2;
    int start_x = (scr_cols - win_w) / 2;

    WINDOW *cw = newwin(win_h, win_w, start_y, start_x);
    draw_box_title(cw, "[ PICTURE ]");

    for(int r=0;r<ROWS;r++){
        for(int c=0;c<COLS;c++){
            char ch = canvas[r][c];
            if(ch=='*')      wattron(cw, COLOR_PAIR(CP_STAR)|A_BOLD);
            else if(ch=='_') wattron(cw, COLOR_PAIR(CP_UNDER)|A_BOLD);
            else             wattron(cw, COLOR_PAIR(CP_CANVAS));
            mvwaddch(cw, r+1, c+1, ch);
            if(ch=='*')      wattroff(cw, COLOR_PAIR(CP_STAR)|A_BOLD);
            else if(ch=='_') wattroff(cw, COLOR_PAIR(CP_UNDER)|A_BOLD);
            else             wattroff(cw, COLOR_PAIR(CP_CANVAS));
        }
    }
    wrefresh(cw);
    show_status(" Press any key to return...");
    wgetch(cw);
    delwin(cw);
    clear();
    refresh();
}

/* ══════════════════════════════════════════════════════
 *  OBJECT LIST WINDOW
 * ══════════════════════════════════════════════════════ */

static void show_list_window(void)
{
    int scr_rows, scr_cols;
    getmaxyx(stdscr, scr_rows, scr_cols);
    int win_h = MAX_OBJECTS + 6, win_w = 56;
    WINDOW *lw = newwin(win_h, win_w,
                        (scr_rows-win_h)/2, (scr_cols-win_w)/2);
    draw_box_title(lw, "[ OBJECT LIST ]");

    wattron(lw, A_UNDERLINE);
    mvwprintw(lw, 2, 2, "%-4s %-10s %-6s  Details", "ID","Type","Sym");
    wattroff(lw, A_UNDERLINE);

    int row=3, found=0;
    for(int i=0;i<obj_count;i++){
        if(!objects[i].active) continue;
        found=1;
        Object *o=&objects[i];
        const char *tn = o->type==OBJ_CIRCLE    ? "Circle"    :
                         o->type==OBJ_RECTANGLE  ? "Rectangle" :
                         o->type==OBJ_LINE       ? "Line"      : "Triangle";
        mvwprintw(lw, row, 2, "%-4d %-10s %-6c  ", i, tn, o->symbol);
        if(o->type==OBJ_CIRCLE)
            wprintw(lw,"ctr(%d,%d) r=%d",o->x1,o->y1,o->r);
        else if(o->type==OBJ_RECTANGLE||o->type==OBJ_LINE)
            wprintw(lw,"(%d,%d)->(%d,%d)",o->x1,o->y1,o->x2,o->y2);
        else
            wprintw(lw,"(%d,%d)(%d,%d)(%d,%d)",o->x1,o->y1,o->x2,o->y2,o->x3,o->y3);
        row++;
    }
    if(!found) mvwprintw(lw, row, 2, "(no active objects)");

    wrefresh(lw);
    show_status(" Press any key to return...");
    wgetch(lw);
    delwin(lw);
    clear(); refresh();
}

/* ══════════════════════════════════════════════════════
 *  INPUT FORM WINDOWS  (one per shape)
 * ══════════════════════════════════════════════════════ */

static void form_add_circle(void)
{
    WINDOW *fw = newwin(12, 40, 5, 20);
    draw_box_title(fw, "[ Add Circle ]");
    wrefresh(fw);

    int cx = win_get_int(fw, 2,  "Centre column (x): ");
    int cy = win_get_int(fw, 3,  "Centre row    (y): ");
    int r  = win_get_int(fw, 4,  "Radius            : ");
    char sym = win_get_sym(fw, 5);

    if(obj_count < MAX_OBJECTS){
        Object o={0};
        o.type=OBJ_CIRCLE; o.active=1;
        o.x1=cx; o.y1=cy; o.r=r; o.symbol=sym;
        objects[obj_count++]=o;
        draw_circle(cx,cy,r,sym);
        show_status(" [OK] Circle added!");
    } else {
        show_status(" [!] Object limit reached!");
    }
    delwin(fw); clear(); refresh();
}

static void form_add_rectangle(void)
{
    WINDOW *fw = newwin(12, 44, 5, 18);
    draw_box_title(fw, "[ Add Rectangle ]");
    wrefresh(fw);

    int x1 = win_get_int(fw, 2, "Top-left  column (x1): ");
    int y1 = win_get_int(fw, 3, "Top-left  row    (y1): ");
    int x2 = win_get_int(fw, 4, "Bot-right column (x2): ");
    int y2 = win_get_int(fw, 5, "Bot-right row    (y2): ");
    char sym = win_get_sym(fw, 6);

    if(obj_count < MAX_OBJECTS){
        Object o={0};
        o.type=OBJ_RECTANGLE; o.active=1;
        o.x1=x1; o.y1=y1; o.x2=x2; o.y2=y2; o.symbol=sym;
        objects[obj_count++]=o;
        draw_rectangle(y1,x1,y2,x2,sym);
        show_status(" [OK] Rectangle added!");
    } else {
        show_status(" [!] Object limit reached!");
    }
    delwin(fw); clear(); refresh();
}

static void form_add_line(void)
{
    WINDOW *fw = newwin(12, 40, 5, 20);
    draw_box_title(fw, "[ Add Line ]");
    wrefresh(fw);

    int x1 = win_get_int(fw, 2, "Start column (x1): ");
    int y1 = win_get_int(fw, 3, "Start row    (y1): ");
    int x2 = win_get_int(fw, 4, "End   column (x2): ");
    int y2 = win_get_int(fw, 5, "End   row    (y2): ");
    char sym = win_get_sym(fw, 6);

    if(obj_count < MAX_OBJECTS){
        Object o={0};
        o.type=OBJ_LINE; o.active=1;
        o.x1=x1; o.y1=y1; o.x2=x2; o.y2=y2; o.symbol=sym;
        objects[obj_count++]=o;
        draw_line(y1,x1,y2,x2,sym);
        show_status(" [OK] Line added!");
    } else {
        show_status(" [!] Object limit reached!");
    }
    delwin(fw); clear(); refresh();
}

static void form_add_triangle(void)
{
    WINDOW *fw = newwin(14, 44, 4, 18);
    draw_box_title(fw, "[ Add Triangle ]");
    wrefresh(fw);

    int x1 = win_get_int(fw, 2, "Vertex-1 column (x1): ");
    int y1 = win_get_int(fw, 3, "Vertex-1 row    (y1): ");
    int x2 = win_get_int(fw, 4, "Vertex-2 column (x2): ");
    int y2 = win_get_int(fw, 5, "Vertex-2 row    (y2): ");
    int x3 = win_get_int(fw, 6, "Vertex-3 column (x3): ");
    int y3 = win_get_int(fw, 7, "Vertex-3 row    (y3): ");
    char sym = win_get_sym(fw, 8);

    if(obj_count < MAX_OBJECTS){
        Object o={0};
        o.type=OBJ_TRIANGLE; o.active=1;
        o.x1=x1; o.y1=y1; o.x2=x2; o.y2=y2; o.x3=x3; o.y3=y3; o.symbol=sym;
        objects[obj_count++]=o;
        draw_triangle(y1,x1,y2,x2,y3,x3,sym);
        show_status(" [OK] Triangle added!");
    } else {
        show_status(" [!] Object limit reached!");
    }
    delwin(fw); clear(); refresh();
}

static void form_delete_object(void)
{
    WINDOW *fw = newwin(8, 36, 8, 22);
    draw_box_title(fw, "[ Delete Object ]");
    mvwprintw(fw, 2, 2, "Active objects: %d", obj_count);
    wrefresh(fw);

    int id = win_get_int(fw, 4, "Enter object ID: ");
    if(id>=0 && id<obj_count && objects[id].active){
        objects[id].active=0;
        redraw_all();
        show_status(" [OK] Object deleted and canvas redrawn.");
    } else {
        show_status(" [!] Invalid object ID.");
    }
    delwin(fw); clear(); refresh();
}

/* ══════════════════════════════════════════════════════
 *  MAIN MENU  (arrow-key navigation)
 * ══════════════════════════════════════════════════════ */

static const char *menu_items[] = {
    "1. Add Circle",
    "2. Add Rectangle",
    "3. Add Line",
    "4. Add Triangle",
    "5. Delete Object",
    "6. List Objects",
    "7. Display Picture",
    "8. Clear Canvas",
    "0. Exit"
};
#define MENU_N (int)(sizeof(menu_items)/sizeof(menu_items[0]))

static void draw_main_menu(WINDOW *mw, int highlight)
{
    werase(mw);
    draw_box_title(mw, "2D TEXT GRAPHICS EDITOR");
    for(int i=0;i<MENU_N;i++){
        if(i==highlight){
            wattron(mw, COLOR_PAIR(CP_SELECT)|A_BOLD);
            mvwprintw(mw, i+2, 2, "  %-28s", menu_items[i]);
            wattroff(mw, COLOR_PAIR(CP_SELECT)|A_BOLD);
        } else {
            wattron(mw, COLOR_PAIR(CP_MENU));
            mvwprintw(mw, i+2, 2, "  %-28s", menu_items[i]);
            wattroff(mw, COLOR_PAIR(CP_MENU));
        }
    }
    mvwprintw(mw, MENU_N+2, 2, "UP/DOWN arrows + ENTER to select");
    wrefresh(mw);
}

/* ══════════════════════════════════════════════════════
 *  ENTRY POINT
 * ══════════════════════════════════════════════════════ */

int main(void)
{
    /* ── init ncurses ── */
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if(!has_colors()){
        endwin();
        fprintf(stderr,"Your terminal does not support colour.\n");
        return 1;
    }
    start_color();
    use_default_colors();

    init_pair(CP_TITLE,  COLOR_YELLOW,  COLOR_BLUE);
    init_pair(CP_BORDER, COLOR_CYAN,    -1);
    init_pair(CP_MENU,   COLOR_WHITE,   -1);
    init_pair(CP_SELECT, COLOR_BLACK,   COLOR_CYAN);
    init_pair(CP_CANVAS, COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_STATUS, COLOR_BLACK,   COLOR_GREEN);
    init_pair(CP_STAR,   COLOR_RED,     COLOR_BLACK);
    init_pair(CP_UNDER,  COLOR_YELLOW,  COLOR_BLACK);

    clear_canvas();

    /* ── menu window ── */
    int scr_rows, scr_cols;
    getmaxyx(stdscr, scr_rows, scr_cols);
    int mh = MENU_N + 5, mw_w = 36;
    WINDOW *mw = newwin(mh, mw_w,
                        (scr_rows-mh)/2,
                        (scr_cols-mw_w)/2);

    int highlight = 0;
    int running   = 1;

    raw();
    keypad(mw, TRUE);
    keypad(stdscr, TRUE);

    show_status(" UP/DOWN or W/S keys, number keys 1-8 | Q to quit");

    while(running){
        draw_main_menu(mw, highlight);

        int key = wgetch(mw);

        /* PDCurses on Windows sometimes sends ESC sequences for arrows */
        if(key == 27){
            nodelay(mw, TRUE);
            int k2 = wgetch(mw);
            int k3 = wgetch(mw);
            nodelay(mw, FALSE);
            if(k2=='[' && k3=='A') key = KEY_UP;
            else if(k2=='[' && k3=='B') key = KEY_DOWN;
        }

        switch(key){
            case KEY_UP:   case 'w': case 'W':
                highlight=(highlight-1+MENU_N)%MENU_N; break;
            case KEY_DOWN: case 's': case 'S':
                highlight=(highlight+1)%MENU_N;        break;

            case '\n': case '\r': case KEY_ENTER: case ' ':
                switch(highlight){
                    case 0: form_add_circle();    break;
                    case 1: form_add_rectangle(); break;
                    case 2: form_add_line();      break;
                    case 3: form_add_triangle();  break;
                    case 4: form_delete_object(); break;
                    case 5: show_list_window();   break;
                    case 6: show_canvas_window(); break;
                    case 7:
                        clear_canvas();
                        obj_count=0;
                        show_status(" [OK] Canvas cleared.");
                        break;
                    case 8: running=0;            break;
                }
                break;

            /* also allow number keys */
            case '1': form_add_circle();    break;
            case '2': form_add_rectangle(); break;
            case '3': form_add_line();      break;
            case '4': form_add_triangle();  break;
            case '5': form_delete_object(); break;
            case '6': show_list_window();   break;
            case '7': show_canvas_window(); break;
            case '8':
                clear_canvas(); obj_count=0;
                show_status(" [OK] Canvas cleared.");
                break;
            case '0': case 'q': case 'Q': running=0; break;
        }
    }

    delwin(mw);
    endwin();
    printf("\nGoodbye!\n");
    return 0;
}