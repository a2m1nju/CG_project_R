#pragma once

#include "CharacterAppearance.h"
#include "Common.h"

void drawCharacter(GLuint shader, const glm::mat4& model, CharacterId id,
                   HeadAccessory head = HeadAccessory::None,
                   BackAccessory back = BackAccessory::None);
