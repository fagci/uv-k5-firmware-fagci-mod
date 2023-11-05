#ifndef ABSCANNER_H
#define ABSCANNER_H

#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/helper.h"
#include <string.h>

void ABSCANNER_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void ABSCANNER_init(void);
void ABSCANNER_update(void);
void ABSCANNER_render(void);

#endif /* end of include guard: ABSCANNER_H */
