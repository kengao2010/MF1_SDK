#include "face_cb.h"

#include "system_config.h"

#include "board.h"
#include "flash.h"
#include "img_op.h"

#if CONFIG_LCD_TYPE_ST7789
#include "lcd_st7789.h"
#elif CONFIG_LCD_TYPE_SSD1963
#include "lcd_ssd1963.h"
#elif CONFIG_LCD_TYPE_SIPEED
#include "lcd_sipeed.h"
#endif

void display_lcd_img_addr(uint32_t pic_addr)
{
#if LCD_240240
    my_w25qxx_read_data(pic_addr, lcd_image, IMG_LCD_SIZE, W25QXX_STANDARD);
    lcd_draw_picture(0, 0, LCD_W, LCD_H, (uint32_t *)lcd_image);
#endif
    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//所有的操作都在dvo输出的图像中进行操作，然后再转换。
void lcd_draw_edge(face_obj_t *obj, uint32_t color)
{
    uint16_t color16 = rgb8882rgb565(color);
    uint32_t *gram = (uint32_t *)&display_image;
    uint32_t data = ((uint32_t)color16 << 16) | (uint32_t)color16;
    uint32_t *addr1, *addr2, *addr3, *addr4, x1, y1, x2, y2;

    x1 = obj->x1;
    y1 = obj->y1;
    x2 = obj->x2;
    y2 = obj->y2;

    if(x1 <= 0)
        x1 = 1;
    if(x2 >= IMG_W - 1)
        x2 = IMG_W - 2;
    if(y1 <= 0)
        y1 = 1;
    if(y2 >= IMG_H - 1)
        y2 = IMG_H - 2;

    addr1 = gram + (IMG_W * y1 + x1) / 2;
    addr2 = gram + (IMG_W * y1 + x2 - 8) / 2;
    addr3 = gram + (IMG_W * (y2 - 1) + x1) / 2;
    addr4 = gram + (IMG_W * (y2 - 1) + x2 - 8) / 2;
    for(uint32_t i = 0; i < 4; i++)
    {
        *addr1 = data;
        *(addr1 + IMG_W / 2) = data;
        *addr2 = data;
        *(addr2 + IMG_W / 2) = data;
        *addr3 = data;
        *(addr3 + IMG_W / 2) = data;
        *addr4 = data;
        *(addr4 + IMG_W / 2) = data;
        addr1++;
        addr2++;
        addr3++;
        addr4++;
    }
    addr1 = gram + (IMG_W * y1 + x1) / 2;
    addr2 = gram + (IMG_W * y1 + x2 - 2) / 2;
    addr3 = gram + (IMG_W * (y2 - 8) + x1) / 2;
    addr4 = gram + (IMG_W * (y2 - 8) + x2 - 2) / 2;
    for(uint32_t i = 0; i < 8; i++)
    {
        *addr1 = data;
        *addr2 = data;
        *addr3 = data;
        *addr4 = data;
        addr1 += IMG_W / 2;
        addr2 += IMG_W / 2;
        addr3 += IMG_W / 2;
        addr4 += IMG_W / 2;
    }
    return;
}

void lcd_draw_false_face(face_recognition_ret_t *ret)
{
    uint32_t face_cnt = ret->result->face_obj_info.obj_number;

    for(uint32_t i = 0; i < face_cnt; i++)
    {
        face_obj_t *obj = &ret->result->face_obj_info.obj[i];
        if(obj->pass == false)
        {
            lcd_draw_edge(obj, 0xff0000);
        }
    }
    return;
}

static uint16_t Fast_AlphaBlender(uint32_t x, uint32_t y, uint32_t Alpha)
{
    x = (x | (x << 16)) & 0x7E0F81F;
    y = (y | (y << 16)) & 0x7E0F81F;
    uint32_t result = ((((x - y) * Alpha) >> 5) + y) & 0x7E0F81F;
    return (uint16_t)((result & 0xFFFF) | (result >> 16));
}

//aplha is stack up picture's diaphaneity
//aplha指的是叠加的图片的透明度
void display_fit_lcd_with_alpha(uint8_t *pic_addr, uint8_t *out_img, uint8_t alpha)
{
    int x0, x1, x, y;
    my_w25qxx_read_data(pic_addr, out_img, IMG_LCD_SIZE, W25QXX_STANDARD);
    uint16_t *s0 = (uint16_t *)display_image;
    uint16_t *s1 = (uint16_t *)out_img;
    x0 = (IMG_W - LCD_W) / 2;
    x1 = x0 + LCD_W;

    for(y = 0; y < LCD_H; y++)
    {
        for(x = 0; x < LCD_W; x++)
        {
            s1[y * LCD_W + x] = Fast_AlphaBlender((uint32_t)s0[y * IMG_W + x + x0], (uint32_t)s1[y * LCD_W + x], (uint32_t)alpha / 8);
        }
    }
    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if CONFIG_LCD_TYPE_ST7789

#if LCD_240240
static void display_fit_lcd(void)
{
    int x0, x1, x, y;
    uint16_t r, g, b;
    uint16_t *src = (uint16_t *)display_image;
    uint16_t *dest = (uint16_t *)lcd_image;
    x0 = (IMG_W - LCD_W) / 2;
    x1 = x0 + LCD_W;
    for(y = 0; y < LCD_H; y++)
    {
        memcpy(lcd_image + y * LCD_W * 2, display_image + (y * IMG_W + x0) * 2, LCD_W * 2);
    }
    return;
}
#endif

void lcd_draw_picture_cb(void)
{
#if LCD_240240
    display_fit_lcd();
    lcd_draw_picture(0, 0, LCD_W, LCD_H, (uint32_t *)lcd_image);
#else
    lcd_draw_picture(0, 0, LCD_W, LCD_H, (uint32_t *)display_image);
#endif
    return;
}

void record_face(face_recognition_ret_t *ret)
{
    uint32_t face_cnt = ret->result->face_obj_info.obj_number;
    get_date_time();
    for(uint32_t i = 0; i < face_cnt; i++)
    {
        if(flash_save_face_info(NULL, ret->result->face_obj_info.obj[i].feature,
                                NULL, 1, NULL, NULL, NULL) < 0) //image, feature, uid, valid, name, note
        {
            printk("Feature Full\n");
            break;
        }
        display_fit_lcd_with_alpha(IMG_RECORD_FACE_ADDR, lcd_image, 128);
        lcd_draw_picture(0, 0, LCD_W, LCD_H, (uint32_t *)lcd_image); //7.5ms
        face_lib_draw_flag = 1;
        set_RGB_LED(GLED);
        msleep(500);
        set_RGB_LED(0);
    }
    return;
}

void lcd_draw_pass(void)
{
    display_fit_lcd_with_alpha(IMG_FACE_PASS_ADDR, lcd_image, 128);
    lcd_draw_picture(0, 0, LCD_W, LCD_H, (uint32_t *)lcd_image); //7.5ms
    face_lib_draw_flag = 1;
    msleep(500); //display
    return;
}

#elif CONFIG_LCD_TYPE_SSD1963
void lcd_convert_cam_to_lcd(void)
{
    return;
}

void lcd_draw_picture_cb(void)
{
    return;
}
void record_face(face_recognition_ret_t *ret)
{
    return;
}

void lcd_draw_pass(void)
{
    return;
}

#else /* CONFIG_LCD_TYPE_SIPEED */

static uint64_t last_dis_tim = 0;

void lcd_draw_picture_cb(void)
{
    uint64_t tim = 0;
    tim = sysctl_get_time_us();
    lcd_covert_cam_to_order((uint32_t *)display_image, (IMG_W * IMG_H / 2));
    // while(dis_flag)
    //     ;
    copy_image_cma_to_lcd(display_image, lcd_image);
    printk("convert img %ld us\r\n", sysctl_get_time_us() - tim);
    return;
}


void record_face(face_recognition_ret_t *ret)
{
    return;
}

void lcd_draw_pass(void)
{
    return;
}
#endif
