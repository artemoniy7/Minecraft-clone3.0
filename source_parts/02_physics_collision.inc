// Физика игрока, проверки земли и разрешение коллизий.
// Этот файл подключается из main2.cpp и рассчитан на сборку в одном translation unit.

bool isSolidBlock(int blockId) {
    return blockId != 0 && blockId != 5;
}

inline bool isSolid(int blockId) {
    return isSolidBlockFast[blockId & 0xFF];
}

bool checkPlayerCollision(const glm::vec3& feetPos) {
    float halfWidth = PLAYER_WIDTH * 0.5f;
    glm::vec3 minCorner = feetPos + glm::vec3(-halfWidth, 0.0f, -halfWidth);
    glm::vec3 maxCorner = feetPos + glm::vec3( halfWidth, PLAYER_HEIGHT,  halfWidth);
    
    int minX = static_cast<int>(std::ceil(minCorner.x - 0.5f));
    int maxX = static_cast<int>(std::floor(maxCorner.x + 0.5f));
    int minY = static_cast<int>(std::ceil(minCorner.y - 0.5f));
    int maxY = static_cast<int>(std::floor(maxCorner.y + 0.5f));
    int minZ = static_cast<int>(std::ceil(minCorner.z - 0.5f));
    int maxZ = static_cast<int>(std::floor(maxCorner.z + 0.5f));
    
    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                int blockId = getBlockAt(x, y, z);
                if (isSolid(blockId)) {
                    glm::vec3 blockMin(x - 0.5f, y - 0.5f, z - 0.5f);
                    glm::vec3 blockMax(x + 0.5f, y + 0.5f, z + 0.5f);
                    if (minCorner.x <= blockMax.x && maxCorner.x >= blockMin.x &&
                        minCorner.y <= blockMax.y && maxCorner.y >= blockMin.y &&
                        minCorner.z <= blockMax.z && maxCorner.z >= blockMin.z) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool isOnGroundCheck(const glm::vec3& feetPos) {
    glm::vec3 below = feetPos;
    below.y -= 0.05f;
    return checkPlayerCollision(below);
}

glm::vec3 applyCollision(const glm::vec3& oldFeetPos, const glm::vec3& delta) {
    glm::vec3 newFeetPos = oldFeetPos;
    glm::vec3 actualDelta(0.0f);
    constexpr float COLLISION_STEP = 0.01f;
    
    // Обработка X
    newFeetPos.x += delta.x;
    if (checkPlayerCollision(newFeetPos)) {
        newFeetPos.x = oldFeetPos.x;
        const float direction = (delta.x > 0.0f) ? 1.0f : -1.0f;
        for (float t = COLLISION_STEP; t <= std::abs(delta.x); t += COLLISION_STEP) {
            newFeetPos.x = oldFeetPos.x + direction * t;
            if (checkPlayerCollision(newFeetPos)) {
                newFeetPos.x -= direction * COLLISION_STEP;
                break;
            }
        }
    }
    actualDelta.x = newFeetPos.x - oldFeetPos.x;
    
    // Обработка Z
    newFeetPos.z += delta.z;
    if (checkPlayerCollision(newFeetPos)) {
        newFeetPos.z = oldFeetPos.z;
        const float direction = (delta.z > 0.0f) ? 1.0f : -1.0f;
        for (float t = COLLISION_STEP; t <= std::abs(delta.z); t += COLLISION_STEP) {
            newFeetPos.z = oldFeetPos.z + direction * t;
            if (checkPlayerCollision(newFeetPos)) {
                newFeetPos.z -= direction * COLLISION_STEP;
                break;
            }
        }
    }
    actualDelta.z = newFeetPos.z - oldFeetPos.z;
    
    // Проверяем, были ли мы в воздухе до обработки Y
    bool wasFalling = playerVelocity.y < 0.0f && !wasOnGround;
    
    // Проверяем, находимся ли мы в воде (по голове или ногам)
    bool inWater = isPlayerInWater();
    
    // Обработка Y
    newFeetPos.y += delta.y;
    if (checkPlayerCollision(newFeetPos)) {
        // Урон от падения — ТОЛЬКО если не в воде
        if (wasFalling && fallDistance > MIN_FALL_DAMAGE_HEIGHT && !inWater) {
            float damage = calculateFallDamage(fallDistance);
            if (damage > 0) {
                applyDamage(static_cast<int>(damage));
            }
        }
        
        // Обнуляем вертикальную скорость при столкновении
        if (delta.y > 0.0f) playerVelocity.y = 0.0f;
        else if (delta.y < 0.0f) playerVelocity.y = 0.0f;
        
        // Сброс расстояния падения при приземлении
        fallDistance = 0.0f;
        
        // Точное размещение на поверхности
        newFeetPos.y = oldFeetPos.y;
        const float direction = (delta.y > 0.0f) ? 1.0f : -1.0f;
        for (float t = COLLISION_STEP; t <= std::abs(delta.y); t += COLLISION_STEP) {
            newFeetPos.y = oldFeetPos.y + direction * t;
            if (checkPlayerCollision(newFeetPos)) {
                newFeetPos.y -= direction * COLLISION_STEP;
                break;
            }
        }
    }
    actualDelta.y = newFeetPos.y - oldFeetPos.y;
    
    // Обновление состояния "на земле"
    bool wasOnGroundPrevious = wasOnGround;
    isOnGround = isOnGroundCheck(newFeetPos);
    
    // Отслеживание дистанции падения
    if (!isOnGround && wasOnGroundPrevious) {
        // Начало падения
        lastGroundY = newFeetPos.y;
        fallDistance = 0.0f;
    } else if (!isOnGround && !wasOnGroundPrevious) {
        // В воздухе - обновляем дистанцию падения (но не в воде)
        if (!inWater) {
            fallDistance = lastGroundY - newFeetPos.y;
        }
    } else if (isOnGround) {
        // На земле - сбрасываем
        fallDistance = 0.0f;
        lastGroundY = newFeetPos.y;
    }
    
    wasOnGround = isOnGround;
    return actualDelta;
}

void placePlayerOnGround() {
    glm::vec3 feetPos = cameraPos - glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    while (feetPos.y > -10.0f && !checkPlayerCollision(feetPos)) {
        feetPos.y -= 0.1f;
    }
    while (feetPos.y < CHUNK_SIZE_Y && checkPlayerCollision(feetPos)) {
        feetPos.y += 0.05f;
    }
    feetPos.y -= 0.05f;
    cameraPos = feetPos + glm::vec3(0.0f, EYE_HEIGHT, 0.0f);
    playerVelocity = glm::vec3(0.0f);
    isOnGround = true;
    wasOnGround = true; // ДОБАВЬТЕ ЭТУ СТРОКУ
    fallDistance = 0.0f; // ДОБАВЬТЕ ЭТУ СТРОКУ
    lastGroundY = feetPos.y; // ДОБАВЬТЕ ЭТУ СТРОКУ
}

