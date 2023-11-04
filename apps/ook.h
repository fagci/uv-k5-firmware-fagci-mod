#ifndef UI_OOK_H
#define UI_OOK_H

#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../ui/helper.h"
#include <string.h>

void OOK_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void OOK_update(void);
void OOK_render(void);

#endif /* end of include guard: OOK_H */
