#pragma once

#include "vk.h"
#include <unordered_set>

struct SemaphorePool {
    std::unordered_set<VkSemaphore> available_semaphores;
    std::unordered_set<VkSemaphore> used_semaphores;

    VkSemaphore acquire_semaphore(VkContext *context) {
        if (available_semaphores.empty()) {
            VkSemaphoreCreateInfo semaphore_create_info = {};
            semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_create_info.flags = VK_SEMAPHORE_TYPE_BINARY;
            VkSemaphore semaphore;
            vkCreateSemaphore(context->device, &semaphore_create_info, nullptr, &semaphore);
            available_semaphores.insert(semaphore);
        }
        VkSemaphore semaphore = *available_semaphores.begin();
        available_semaphores.erase(semaphore);
        used_semaphores.insert(semaphore);
        return semaphore;
    }

    void release_semaphore(VkSemaphore semaphore) {
        used_semaphores.erase(semaphore);
        available_semaphores.insert(semaphore);
    }

    void cleanup(VkContext *context) {
        for (auto &semaphore: available_semaphores) {
            vkDestroySemaphore(context->device, semaphore, nullptr);
        }
        available_semaphores.clear();
        for (auto &semaphore: used_semaphores) {
            vkDestroySemaphore(context->device, semaphore, nullptr);
        }
        used_semaphores.clear();
    }
};
