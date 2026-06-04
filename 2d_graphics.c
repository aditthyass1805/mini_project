/*
 * ============================================================
 *   2D Text Graphics Editor in C
 *   Uses a 2D char canvas with * and _
 *   Supports: Circle, Rectangle, Line, Triangle
 *   Features: Add, Delete, Display objects
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -- Canvas dimensions -- */
#define ROWS 30
#define COLS 80

/* -- Maximum objects in the picture -- */
#define MAX_OBJECTS 20

/* -- Object type codes -- */
#define OBJ_CIRCLE    1
#define OBJ_RECTANGLE 2
#define OBJ_LINE      3
#define OBJ_TRIANGLE  4

/* -- 2D canvas (global picture buffer) -- */
char canvas[ROWS][COLS];

/* ------------------------------------------──
 *  Object descriptor – stores enough data to
 *  re-draw or erase any shape.
 * -------------------------------------------- */
typedef struct {
    int  type;          /* OBJ_* constant        */
    int  active;        /* 1 = alive, 0 = deleted */
    /* Generic parameters (meaning varies by type) */
    int  x1, y1;        /* origin / start point   */
    int  x2, y2;        /* end point / size       */
    int  r;             /* radius (circle)        */
    char symbol;        /* '*' or '_'             */
    char label[16];     /* user-friendly name     */
} Object;

Object objects[MAX_OBJECTS];
int    obj_count = 0;   /* next free slot index  */

/* ============================================
 *  CANVAS UTILITIES
 * ============================================ */

/* Fill canvas with spaces */
void clear_canvas(void)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            canvas[r][c] = ' ';
}

/* Safe pixel setter – ignores out-of-bounds */
void set_pixel(int r, int c, char ch)
{
    if (r >= 0 && r < ROWS && c >= 0 && c < COLS)
        canvas[r][c] = ch;
}

/* Print the canvas with a border */
void display_canvas(void)
{
    printf("\n");
    /* Top border */
    printf("+");
    for (int c = 0; c < COLS; c++) printf("-");
    printf("+\n");

    for (int r = 0; r < ROWS; r++) {
        printf("|");
        for (int c = 0; c < COLS; c++)
            putchar(canvas[r][c]);
        printf("|\n");
    }

    /* Bottom border */
    printf("+");
    for (int c = 0; c < COLS; c++) printf("-");
    printf("+\n");
}

/* ============================================
 *  DRAWING PRIMITIVES
 * ============================================ */

/*
 * draw_circle  – Bresenham midpoint circle algorithm
 *   (cx, cy) centre;  radius r;  ch = fill character
 */
void draw_circle(int cx, int cy, int r, char ch)
{
    /* We scale x by 2 to compensate for char cell aspect ratio */
    int x = 0, y = r;
    int d = 1 - r;

    while (x <= y) {
        /* Plot 8 symmetric points (x scaled ×2) */
        set_pixel(cy + y, cx + 2 * x, ch);
        set_pixel(cy + y, cx - 2 * x, ch);
        set_pixel(cy - y, cx + 2 * x, ch);
        set_pixel(cy - y, cx - 2 * x, ch);
        set_pixel(cy + x, cx + 2 * y, ch);
        set_pixel(cy + x, cx - 2 * y, ch);
        set_pixel(cy - x, cx + 2 * y, ch);
        set_pixel(cy - x, cx - 2 * y, ch);

        if (d < 0)
            d += 2 * x + 3;
        else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/*
 * draw_line  – Bresenham line between (r1,c1) and (r2,c2)
 */
void draw_line(int r1, int c1, int r2, int c2, char ch)
{
    int dr = abs(r2 - r1), dc = abs(c2 - c1);
    int sr = (r1 < r2) ? 1 : -1;
    int sc = (c1 < c2) ? 1 : -1;
    int err = dr - dc, e2;

    for (;;) {
        set_pixel(r1, c1, ch);
        if (r1 == r2 && c1 == c2) break;
        e2 = 2 * err;
        if (e2 > -dc) { err -= dc; r1 += sr; }
        if (e2 <  dr) { err += dr; c1 += sc; }
    }
}

/*
 * draw_rectangle  – axis-aligned hollow rectangle
 *   top-left (r1,c1), bottom-right (r2,c2)
 */
void draw_rectangle(int r1, int c1, int r2, int c2, char ch)
{
    /* Normalise so r1 <= r2, c1 <= c2 */
    if (r1 > r2) { int t = r1; r1 = r2; r2 = t; }
    if (c1 > c2) { int t = c1; c1 = c2; c2 = t; }

    for (int c = c1; c <= c2; c++) {
        set_pixel(r1, c, ch);   /* top    */
        set_pixel(r2, c, ch);   /* bottom */
    }
    for (int r = r1; r <= r2; r++) {
        set_pixel(r, c1, ch);   /* left   */
        set_pixel(r, c2, ch);   /* right  */
    }
}

/*
 * draw_triangle  – three vertices connected by Bresenham lines
 *   Vertices: (r1,c1), (r2,c2), (r3,c3)
 */
void draw_triangle(int r1, int c1,
                   int r2, int c2,
                   int r3, int c3, char ch)
{
    draw_line(r1, c1, r2, c2, ch);
    draw_line(r2, c2, r3, c3, ch);
    draw_line(r3, c3, r1, c1, ch);
}

/* ============================================
 *  OBJECT MANAGEMENT
 * ============================================ */

/* Rebuild the whole canvas from the active objects list */
void redraw_all(void)
{
    clear_canvas();
    for (int i = 0; i < obj_count; i++) {
        if (!objects[i].active) continue;
        Object *o = &objects[i];
        switch (o->type) {
            case OBJ_CIRCLE:
                draw_circle(o->x1, o->y1, o->r, o->symbol);
                break;
            case OBJ_RECTANGLE:
                draw_rectangle(o->x1, o->y1, o->x2, o->y2, o->symbol);
                break;
            case OBJ_LINE:
                draw_line(o->x1, o->y1, o->x2, o->y2, o->symbol);
                break;
            case OBJ_TRIANGLE:
                draw_triangle(o->x1, o->y1,
                              o->x2, o->y2,
                              o->r,  o->symbol, /* r,symbol used as x3,y3 */
                              o->label[0]);     /* label[0] used as char  */
                break;
        }
    }
}

/* Add a new object descriptor; returns its index or -1 */
int add_object(Object obj)
{
    if (obj_count >= MAX_OBJECTS) {
        printf("[!] Object limit (%d) reached.\n", MAX_OBJECTS);
        return -1;
    }
    objects[obj_count] = obj;
    objects[obj_count].active = 1;
    return obj_count++;
}

/* List all active objects */
void list_objects(void)
{
    int found = 0;
    printf("\n%-4s %-12s %-8s Details\n", "ID", "Type", "Symbol");
    printf("--------------------------------------------\n");
    for (int i = 0; i < obj_count; i++) {
        if (!objects[i].active) continue;
        found = 1;
        Object *o = &objects[i];
        const char *tname = (o->type == OBJ_CIRCLE)    ? "Circle"    :
                            (o->type == OBJ_RECTANGLE)  ? "Rectangle" :
                            (o->type == OBJ_LINE)       ? "Line"      : "Triangle";
        printf("%-4d %-12s %-8c ", i, tname, o->symbol);
        if (o->type == OBJ_CIRCLE)
            printf("centre(%d,%d) r=%d", o->y1, o->x1, o->r);
        else if (o->type == OBJ_RECTANGLE)
            printf("(%d,%d) to (%d,%d)", o->y1, o->x1, o->y2, o->x2);
        else if (o->type == OBJ_LINE)
            printf("(%d,%d) to (%d,%d)", o->y1, o->x1, o->y2, o->x2);
        else
            printf("(%d,%d),(%d,%d),(%d,%d)",
                   o->y1, o->x1, o->y2, o->x2, o->symbol, o->r);
        printf("\n");
    }
    if (!found) printf("  (no active objects)\n");
    printf("\n");
}

/* Delete an object by index */
void delete_object(int idx)
{
    if (idx < 0 || idx >= obj_count || !objects[idx].active) {
        printf("[!] Invalid object ID.\n");
        return;
    }
    objects[idx].active = 0;
    printf("[OK] Object %d deleted.\n", idx);
    redraw_all();
}

/* ============================================
 *  INPUT HELPERS
 * ============================================ */

static int get_int(const char *prompt)
{
    int v;
    printf("%s", prompt);
    scanf("%d", &v);
    return v;
}

static char get_symbol(void)
{
    char sym;
    printf("  Symbol (* or _): ");
    scanf(" %c", &sym);
    if (sym != '*' && sym != '_') sym = '*';
    return sym;
}

/* ============================================
 *  ADD-OBJECT MENUS
 * ============================================ */

void menu_add_circle(void)
{
    Object o = {0};
    o.type = OBJ_CIRCLE;
    printf("\n--- Add Circle ---\n");
    printf("  (col=x, row=y; top-left is 0,0)\n");
    o.x1 = get_int("  Centre column (x): ");
    o.y1 = get_int("  Centre row    (y): ");
    o.r  = get_int("  Radius            : ");
    o.symbol = get_symbol();
    int id = add_object(o);
    if (id >= 0) {
        draw_circle(o.x1, o.y1, o.r, o.symbol);
        printf("[OK] Circle added as object %d.\n", id);
    }
}

void menu_add_rectangle(void)
{
    Object o = {0};
    o.type = OBJ_RECTANGLE;
    printf("\n--- Add Rectangle ---\n");
    o.x1 = get_int("  Top-left  column (x1): ");
    o.y1 = get_int("  Top-left  row    (y1): ");
    o.x2 = get_int("  Bot-right column (x2): ");
    o.y2 = get_int("  Bot-right row    (y2): ");
    o.symbol = get_symbol();
    int id = add_object(o);
    if (id >= 0) {
        draw_rectangle(o.y1, o.x1, o.y2, o.x2, o.symbol);
        printf("[OK] Rectangle added as object %d.\n", id);
    }
}

void menu_add_line(void)
{
    Object o = {0};
    o.type = OBJ_LINE;
    printf("\n--- Add Line ---\n");
    o.x1 = get_int("  Start column (x1): ");
    o.y1 = get_int("  Start row    (y1): ");
    o.x2 = get_int("  End   column (x2): ");
    o.y2 = get_int("  End   row    (y2): ");
    o.symbol = get_symbol();
    int id = add_object(o);
    if (id >= 0) {
        draw_line(o.y1, o.x1, o.y2, o.x2, o.symbol);
        printf("[OK] Line added as object %d.\n", id);
    }
}

void menu_add_triangle(void)
{
    /* Reuse fields: (x1,y1),(x2,y2),(r,symbol-as-int) for 3rd vertex
     * label[0] = draw character
     * This is a deliberate compact encoding to stay within the struct */
    Object o = {0};
    o.type = OBJ_TRIANGLE;
    printf("\n--- Add Triangle (3 vertices) ---\n");
    int c1 = get_int("  Vertex-1 column (x1): ");
    int r1 = get_int("  Vertex-1 row    (y1): ");
    int c2 = get_int("  Vertex-2 column (x2): ");
    int r2 = get_int("  Vertex-2 row    (y2): ");
    int c3 = get_int("  Vertex-3 column (x3): ");
    int r3 = get_int("  Vertex-3 row    (y3): ");
    char sym = get_symbol();

    /* Pack into Object fields */
    o.x1 = c1; o.y1 = r1;
    o.x2 = c2; o.y2 = r2;
    o.r  = r3; o.symbol = (char)c3;   /* c3 stored in symbol as int */
    o.label[0] = sym;                  /* actual draw char           */

    int id = add_object(o);
    if (id >= 0) {
        draw_triangle(r1, c1, r2, c2, r3, c3, sym);
        printf("[OK] Triangle added as object %d.\n", id);
    }
}

/* ============================================
 *  MAIN MENU
 * ============================================ */

void print_banner(void)
{
    printf("\n");
    printf("+======================================+\n");
    printf("|     2D TEXT GRAPHICS EDITOR v1.0     |\n");
    printf("|   shapes: * and _ | canvas %dx%d     |\n", COLS, ROWS);
    printf("+======================================+\n");
}

void print_menu(void)
{
    printf("\n");
    printf("+------------- MAIN MENU -------------+\n");
    printf("| 1. Add Circle                       |\n");
    printf("| 2. Add Rectangle                    |\n");
    printf("| 3. Add Line                         |\n");
    printf("| 4. Add Triangle                     |\n");
    printf("| 5. Delete Object                    |\n");
    printf("| 6. List Objects                     |\n");
    printf("| 7. Display Picture                  |\n");
    printf("| 8. Clear Canvas                     |\n");
    printf("| 0. Exit                             |\n");
    printf("+-------------------------------------+\n");
    printf("Choice: ");
}

int main(void)
{
    clear_canvas();
    print_banner();

    int choice;
    do {
        print_menu();
        scanf("%d", &choice);

        switch (choice) {
            case 1: menu_add_circle();                         break;
            case 2: menu_add_rectangle();                      break;
            case 3: menu_add_line();                           break;
            case 4: menu_add_triangle();                       break;
            case 5:
                list_objects();
                {
                    int id = get_int("  Enter object ID to delete: ");
                    delete_object(id);
                }
                break;
            case 6: list_objects();                            break;
            case 7: display_canvas();                          break;
            case 8:
                clear_canvas();
                obj_count = 0;
                printf("[OK] Canvas cleared.\n");
                break;
            case 0: printf("\n  Goodbye!\n\n");                break;
            default: printf("[!] Invalid choice.\n");          break;
        }
    } while (choice != 0);

    return 0;
}