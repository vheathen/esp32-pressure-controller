#ifndef _UI_H_
#define _UI_H_

#define PRIMARY_COLOR 0x8069       // #880e4f
#define PRIMARY_LIGHT_COLOR 0xB22E // #bc477b
#define PRIMARY_DARK_COLOR 0x5004  // #560027

#define SECONDARY_COLOR 0x0267       // #004d40
#define SECONDARY_LIGHT_COLOR 0x33AD // #39796b
#define SECONDARY_DARK_COLOR 0x0123  // #00251a

#define BRIGHT_TEXT_COLOR WHITE
#define DIM_TEXT_COLOR 0x451D // #42a5f5

#define LIGHT_BACKGROUND_COLOR 0xE75C // #eeeeee

void ui_task(void *pvParameters);

#endif // _UI_H_