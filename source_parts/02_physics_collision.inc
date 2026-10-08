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
    
    // Разрешаем горизонтальные оси через двоичный поиск.
    // В отличие от фиксированного шага 0.01, результат не зависит от величины delta.
    auto resolveHorizontalAxis = [&](int axis, float oldValue, float deltaValue) -> float {
        if (std::abs(deltaValue) < 1e-6f) {
            return oldValue;
        }

        glm::vec3 candidate = newFeetPos;
        if (axis == 0) {
            candidate.x = oldValue + deltaValue;
        } else {
            candidate.z = oldValue + deltaValue;
        }

        if (!checkPlayerCollision(candidate)) {
            return oldValue + deltaValue;
        }

        const float direction = (deltaValue > 0.0f) ? 1.0f : -1.0f;
        float lo = 0.0f;
        float hi = std::abs(deltaValue);

        for (int i = 0; i < 16; ++i) {
            const float mid = (lo + hi) * 0.5f;
            candidate = newFeetPos;
            if (axis == 0) {
                candidate.x = oldValue + direction * mid;
            } else {
                candidate.z = oldValue + direction * mid;
            }

            if (checkPlayerCollision(candidate)) {
                hi = mid;
            } else {
                lo = mid;
            }
        }

        constexpr float CONTACT_EPSILON = 0.0005f;
        return oldValue + direction * std::max(0.0f, lo - CONTACT_EPSILON);
    };

    // Обработка X
    newFeetPos.x = resolveHorizontalAxis(0, oldFeetPos.x, delta.x);
    actualDelta.x = newFeetPos.x - oldFeetPos.x;

    // Обработка Z
    newFeetPos.z = resolveHorizontalAxis(1, oldFeetPos.z, delta.z);
    actualDelta.z = newFeetPos.z - oldFeetPos.z;
    
    bool wasFalling = playerVelocity.y < 0.0f && !wasOnGround;
    bool inWater = isPlayerInWater();
    
    // Обработка Y
    newFeetPos.y += delta.y;
    if (checkPlayerCollision(newFeetPos)) {
        if (wasFalling && fallDistance > MIN_FALL_DAMAGE_HEIGHT && !inWater) {
            float damage = calculateFallDamage(fallDistance);
            if (damage > 0) {
                applyDamage(static_cast<int>(damage));
            }
        }
        
        playerVelocity.y = 0.0f;
        fallDistance = 0.0f;
        
        // Ищем точную последнюю безопасную позицию вместо серии
        // дискретных откатов по 0.01, которые и давали дрожание камеры.
        newFeetPos.y = oldFeetPos.y;
        const float direction = (delta.y > 0.0f) ? 1.0f : -1.0f;
        float lo = 0.0f;
        float hi = std::abs(delta.y);

        for (int i = 0; i < 16; ++i) {
            const float mid = (lo + hi) * 0.5f;
            newFeetPos.y = oldFeetPos.y + direction * mid;
            if (checkPlayerCollision(newFeetPos)) {
                hi = mid;
            } else {
                lo = mid;
            }
        }

        constexpr float CONTACT_EPSILON = 0.0005f;
        newFeetPos.y = oldFeetPos.y +
            direction * std::max(0.0f, lo - CONTACT_EPSILON);
    }
    actualDelta.y = newFeetPos.y - oldFeetPos.y;
    
    bool wasOnGroundPrevious = wasOnGround;
    isOnGround = isOnGroundCheck(newFeetPos);
    
    if (!isOnGround && wasOnGroundPrevious) {
        lastGroundY = newFeetPos.y;
        fallDistance = 0.0f;
    } else if (!isOnGround && !wasOnGroundPrevious) {
        if (!inWater) {
            fallDistance = lastGroundY - newFeetPos.y;
        }
    } else if (isOnGround) {
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
    wasOnGround = true;
    fallDistance = 0.0f;
    lastGroundY = feetPos.y;
}

