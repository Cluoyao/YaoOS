int   api_openwin(char *buf, int xsiz, int ysiz, int col_inv, char *title);
void  api_end(void);
void  api_linewin(int win, int x0, int y0, int x1, int y1, int col);
void  api_refreshwin(int win, int x0, int y0, int x1, int y1);
void  api_initmalloc(void);
char *api_malloc(int size);
int   api_getkey(int mode);

void HariMain(void)
{
    char *buf;
    int   win, x, y, r, g, b;
    api_initmalloc();
    buf = api_malloc(144 * 164);
    win = api_openwin(buf, 144, 164, -1, "color");
    /*窗口左侧宽8，上侧菜单栏高28*/
    for(y = 0; y < 128; y++)
    {
        for(x = 0; x < 128; x++)
        {
            r = x * 2;
            g = y * 2;
            b = 0;
            /*buf[(x + 8) + (y + 28) * 144]逐元素遍历*/
            buf[(x + 8) + (y + 28) * 144] = 16 + (r / 43) * 1 + (g / 43) * 6 + (b / 43) * 36;
        }
    }
    api_refreshwin(win, 8, 28, 136, 156);
    api_getkey(1);
    api_end();
}