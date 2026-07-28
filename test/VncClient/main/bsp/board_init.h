// board_init.h
//

#ifdef __cplusplus
extern "C"
{
#endif


//
//
//

void bsp_init_gpio();
void bsp_init_lcd();
void bsp_init_touch();


//
//
//

void lcd_draw_bitmap(int x, int y, int w, int h, uint8_t* data);
void lcd_backlight(bool on);
void lcd_register_event_callbacks();



#ifdef __cplusplus
}
#endif
