#ifndef SCANLIST_H
#define SCANLIST_H

#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../ui/helper.h"
#include <string.h>

void SCANLIST_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void SCANLIST_update(void);
void SCANLIST_render(void);

#endif /* end of include guard: SCANLIST_H */
