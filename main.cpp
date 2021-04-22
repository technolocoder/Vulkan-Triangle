#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
using namespace std;

FILE *debug_file = fopen("debug.txt","w");

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_flags ,const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data){
    fprintf(debug_file,"%s\n",callback_data->pMessage);
    fflush(debug_file);
    return VK_FALSE;
}

VkResult create_debug_messenger(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_info ,const VkAllocationCallbacks *allocator, VkDebugUtilsMessengerEXT *debug_messenger){
    auto function = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
    if(function) return function(instance,debug_messenger_info,allocator,debug_messenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void destroy_debug_messenger(VkInstance instance ,VkDebugUtilsMessengerEXT debug_messenger ,const VkAllocationCallbacks *allocator){
    auto function = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
    function(instance,debug_messenger,allocator);
}

VkShaderModule load_shader(const char *shaderpath, VkDevice logical_device){
    FILE *file = fopen(shaderpath,"rb");
    if(!file) throw runtime_error("Error opening shader path");
    fseek(file,0,SEEK_END);
    unsigned int size = ftell(file);
    fseek(file,0,SEEK_SET);
    char data[size];
    fread(data,1,size,file);
    VkShaderModuleCreateInfo module_info {};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.pNext = nullptr;
    module_info.pCode = reinterpret_cast<unsigned int*>(data);
    module_info.flags = 0;
    module_info.codeSize = size;
    VkShaderModule shadermodule;
    if(vkCreateShaderModule(logical_device,&module_info,nullptr,&shadermodule)!=VK_SUCCESS) throw runtime_error("Error creating shader module");
    fclose(file);
    return shadermodule;
}

int main(){
    try{
        SDL_Init(SDL_INIT_VIDEO);
        const unsigned int window_width=500,window_height=500;
        SDL_Window *window = SDL_CreateWindow("Test Triangle",0,0,window_width,window_height,SDL_WINDOW_VULKAN);

        unsigned int instance_extension_count, instance_layer_count = 1;
        SDL_Vulkan_GetInstanceExtensions(window,&instance_extension_count,nullptr);
        const char *instance_extensions[instance_extension_count+1],*instance_layers[instance_layer_count] = {"VK_LAYER_KHRONOS_validation"};
        SDL_Vulkan_GetInstanceExtensions(window,&instance_extension_count,instance_extensions);
        instance_extensions[instance_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

        VkApplicationInfo app_info {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Test Triangle";
        app_info.pEngineName = "No Engine";
        app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
        app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
        app_info.apiVersion = VK_API_VERSION_1_2;
        app_info.pNext = nullptr;

        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info {};
        debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_messenger_info.pNext = nullptr;
        debug_messenger_info.pUserData = nullptr;
        debug_messenger_info.pfnUserCallback = debug_messenger_callback;
        debug_messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        debug_messenger_info.flags = 0;

        VkInstanceCreateInfo instance_info {};
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.ppEnabledLayerNames = instance_layers;
        instance_info.ppEnabledExtensionNames = instance_extensions;
        instance_info.pApplicationInfo = &app_info;
        instance_info.pNext = &debug_messenger_info;
        instance_info.flags = 0;
        instance_info.enabledLayerCount = instance_layer_count;
        instance_info.enabledExtensionCount = instance_extension_count;

        VkInstance instance;
        if(vkCreateInstance(&instance_info,nullptr,&instance)!=VK_SUCCESS) throw runtime_error("Error creating instance");

        VkDebugUtilsMessengerEXT debug_messenger;
        if(create_debug_messenger(instance,&debug_messenger_info,nullptr,&debug_messenger)!=VK_SUCCESS) throw runtime_error("Error creating debug messenger");

        unsigned int physical_device_count;
        vkEnumeratePhysicalDevices(instance,&physical_device_count,nullptr);
        VkPhysicalDevice physical_devices[physical_device_count];
        vkEnumeratePhysicalDevices(instance,&physical_device_count,physical_devices);

        //TODO implement physical device selection
        VkPhysicalDevice physical_device = physical_devices[0];

        VkSurfaceKHR surface;
        SDL_Vulkan_CreateSurface(window,instance,&surface);


        unsigned int queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&queue_family_count,nullptr);
        VkQueueFamilyProperties queue_families[queue_family_count];
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&queue_family_count,queue_families);

        unsigned int render_present_queue_index = -1;
        for(int i = 0; i < queue_family_count; ++i){
            if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
                VkBool32 surface_support;
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,i,surface,&surface_support);

                if(surface_support){
                    render_present_queue_index = i;
                    break;
                }
            }
        }

        if(render_present_queue_index<0) throw runtime_error("No Surface Support");
        
        VkSurfaceCapabilitiesKHR surface_capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device,surface,&surface_capabilities);

        unsigned int present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,surface,&present_mode_count,nullptr);
        VkPresentModeKHR present_modes[present_mode_count];
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,surface,&present_mode_count,present_modes);

        unsigned int surface_format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,surface,&surface_format_count,nullptr);
        VkSurfaceFormatKHR surface_formats[surface_format_count];
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,surface,&surface_format_count,surface_formats);

        VkPresentModeKHR present_mode;
        for(int i = 0; i < present_mode_count; ++i){
            if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR){
                present_mode = present_modes[i];
                goto skip_present;
            }
        }
        present_mode = present_modes[0];
        skip_present:;

        VkSurfaceFormatKHR surface_format;
        for(int i = 0; i < surface_format_count; ++i){
            if(surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && surface_formats[i].format == VK_FORMAT_B8G8R8_SRGB){
                surface_format = surface_formats[i];
                goto skip_surface;
            }
        }
        surface_format = surface_formats[0];
        skip_surface:;

        unsigned int min_swapchain_image_count = surface_capabilities.minImageCount;
        VkExtent2D surface_extent;
        if(surface_capabilities.currentExtent.width==UINT64_MAX){
            surface_extent.width  = max(surface_capabilities.minImageExtent.width,min(surface_capabilities.maxImageExtent.width,window_width));
            surface_extent.height = max(surface_capabilities.minImageExtent.height,min(surface_capabilities.maxImageExtent.height,window_height));
        }else{  
            surface_extent = surface_capabilities.currentExtent;
        }

        float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo device_queue_info {};
        device_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        device_queue_info.pQueuePriorities = &queue_priority;
        device_queue_info.queueCount = 1;
        device_queue_info.queueFamilyIndex = render_present_queue_index; 
        device_queue_info.pNext = nullptr;
        device_queue_info.flags = 0;

        const unsigned int device_extension_count = 1;
        const char *device_extensions[device_extension_count] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo device_info {};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &device_queue_info;
        device_info.ppEnabledLayerNames = nullptr;
        device_info.ppEnabledExtensionNames = device_extensions;
        device_info.pNext = nullptr;
        device_info.pEnabledFeatures = nullptr;
        device_info.flags = 0;
        device_info.enabledLayerCount = 0;
        device_info.enabledExtensionCount = device_extension_count;

        VkDevice logical_device;
        if(vkCreateDevice(physical_device,&device_info,nullptr,&logical_device)!=VK_SUCCESS) throw runtime_error("Error creating device");

        VkPipelineVertexInputStateCreateInfo vertex_input {};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.pNext = nullptr;
        vertex_input.pVertexAttributeDescriptions = nullptr;
        vertex_input.pVertexBindingDescriptions = nullptr;
        vertex_input.vertexAttributeDescriptionCount = 0;
        vertex_input.vertexBindingDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo input_assembly {};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.flags = 0;
        input_assembly.pNext = nullptr;
        input_assembly.primitiveRestartEnable = VK_FALSE;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport {};
        viewport.height = surface_extent.height;
        viewport.width = surface_extent.width;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        viewport.x = 0;
        viewport.y = 0;

        VkRect2D scissor {};
        scissor.offset = {0,0};
        scissor.extent = surface_extent;

        VkPipelineViewportStateCreateInfo viewport_state {};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.pScissors = &scissor;
        viewport_state.pNext = nullptr;
        viewport_state.flags = 0;

        VkPipelineRasterizationStateCreateInfo rasterizer {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.pNext = nullptr;
        rasterizer.lineWidth = 1.0f;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.flags = 0;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment {};
        color_blend_attachment.blendEnable = VK_FALSE;
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_A_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_R_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend_state {};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.pNext = nullptr;
        color_blend_state.pAttachments = &color_blend_attachment;
        color_blend_state.logicOpEnable = VK_FALSE;
        color_blend_state.flags = 0;
        color_blend_state.attachmentCount = 1;

        VkPipelineMultisampleStateCreateInfo multisample_info {};
        multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_info.sampleShadingEnable = VK_FALSE;
        multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_info.pSampleMask = nullptr;
        multisample_info.pNext = nullptr;
        multisample_info.minSampleShading = 0;
        multisample_info.flags = 0;
        multisample_info.alphaToOneEnable = VK_FALSE;
        multisample_info.alphaToCoverageEnable = VK_FALSE;

        VkAttachmentDescription attachment_description {};
        attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment_description.format = surface_format.format;
        attachment_description.flags = 0;
        attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference attachment_reference {};
        attachment_reference.attachment = 0;
        attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description {};
        subpass_description.colorAttachmentCount = 1;
        subpass_description.flags = 0;
        subpass_description.pColorAttachments = &attachment_reference;
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency subpass_dependency {};
        subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dependency.dstSubpass = 0;
        subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.srcAccessMask = 0;
        subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderpass_info {};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderpass_info.subpassCount = 1;
        renderpass_info.pSubpasses = &subpass_description;
        renderpass_info.pNext = nullptr;
        renderpass_info.pDependencies = &subpass_dependency;
        renderpass_info.pAttachments = &attachment_description;
        renderpass_info.flags = 0;
        renderpass_info.dependencyCount = 1;
        renderpass_info.pDependencies = &subpass_dependency;
        renderpass_info.attachmentCount = 1;

        VkRenderPass renderpass;
        if(vkCreateRenderPass(logical_device,&renderpass_info,nullptr,&renderpass)!=VK_SUCCESS) throw runtime_error("Error creating renderpass");

        VkPipelineLayoutCreateInfo layout_info {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 0;
        layout_info.pushConstantRangeCount = 0;
        layout_info.pSetLayouts = nullptr;
        layout_info.pPushConstantRanges = nullptr;
        layout_info.pNext = nullptr;
        layout_info.flags = 0;

        VkPipelineLayout pipeline_layout;
        if(vkCreatePipelineLayout(logical_device,&layout_info,nullptr,&pipeline_layout)!=VK_SUCCESS) throw runtime_error("Error creating pipeline layout");

        VkShaderModule vs_module = load_shader("shader-bin/vs.spv",logical_device), fs_module = load_shader("shader-bin/fs.spv",logical_device);

        VkPipelineShaderStageCreateInfo vs{},fs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vs.flags = 0;
        vs.module = vs_module;
        vs.pName = "main";
        vs.pNext = nullptr;
        vs.pSpecializationInfo = nullptr;
        vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
        
        fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fs.flags = 0;
        fs.module = fs_module;
        fs.pName = "main";
        fs.pNext = nullptr;
        fs.pSpecializationInfo = nullptr;
        fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkPipelineShaderStageCreateInfo shader_stages[2] = {vs,fs};

        VkGraphicsPipelineCreateInfo pipeline_info {};
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pMultisampleState = &multisample_info;
        pipeline_info.pNext = nullptr;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pTessellationState = nullptr;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.renderPass = renderpass;
        pipeline_info.stageCount = 2;
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.pColorBlendState = &color_blend_state;
        pipeline_info.pDepthStencilState = nullptr;
        pipeline_info.pDynamicState = nullptr;

        VkPipeline pipeline;
        if(vkCreateGraphicsPipelines(logical_device,VK_NULL_HANDLE,1,&pipeline_info,nullptr,&pipeline)!=VK_SUCCESS) throw runtime_error("Error creating pipeline");

        VkSwapchainCreateInfoKHR swapchain_info {};
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.surface = surface;
        swapchain_info.queueFamilyIndexCount = 1;
        swapchain_info.preTransform = surface_capabilities.currentTransform;
        swapchain_info.presentMode = present_mode;
        swapchain_info.pQueueFamilyIndices = &render_present_queue_index;
        swapchain_info.pNext = nullptr;
        swapchain_info.oldSwapchain = VK_NULL_HANDLE;
        swapchain_info.minImageCount = min_swapchain_image_count;
        swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.imageFormat = surface_format.format;
        swapchain_info.imageExtent = surface_extent;
        swapchain_info.imageColorSpace = surface_format.colorSpace;
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.flags = 0;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.clipped = VK_TRUE;

        vkDestroyShaderModule(logical_device,vs_module,nullptr);
        vkDestroyShaderModule(logical_device,fs_module,nullptr);
        unsigned int swapchain_image_count;

        VkSwapchainKHR swapchain;
        if(vkCreateSwapchainKHR(logical_device,&swapchain_info,nullptr,&swapchain)!=VK_SUCCESS) throw runtime_error("Error creating swapchain");

        vkGetSwapchainImagesKHR(logical_device,swapchain,&swapchain_image_count,nullptr);
        VkImage swapchain_images[swapchain_image_count];
        VkImageView swapchain_image_views[swapchain_image_count];
        VkFramebuffer swapchain_framebuffers[swapchain_image_count];
        vkGetSwapchainImagesKHR(logical_device,swapchain,&swapchain_image_count,swapchain_images);
        
        VkImageViewCreateInfo image_view_info {};
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.flags = 0;
        image_view_info.format = surface_format.format;
        image_view_info.pNext = nullptr;
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.layerCount = 1;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

        VkFramebufferCreateInfo framebuffer_info {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.width = surface_extent.width;
        framebuffer_info.renderPass = renderpass;
        framebuffer_info.pNext = nullptr;
        framebuffer_info.layers = 1;
        framebuffer_info.height = surface_extent.height;
        framebuffer_info.flags = 0;
        framebuffer_info.attachmentCount = 1;

        for(int i = 0; i < swapchain_image_count; ++i){
            image_view_info.image = swapchain_images[i];
            if(vkCreateImageView(logical_device,&image_view_info,nullptr,&swapchain_image_views[i])!=VK_SUCCESS) throw runtime_error("Error creating image view");
            framebuffer_info.pAttachments = &swapchain_image_views[i];
            if(vkCreateFramebuffer(logical_device,&framebuffer_info,nullptr,&swapchain_framebuffers[i])!=VK_SUCCESS) throw runtime_error("Error creating framebuffer");
        }

        VkCommandPoolCreateInfo commandpool_info {};
        commandpool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandpool_info.queueFamilyIndex = render_present_queue_index;
        commandpool_info.pNext = nullptr;
        commandpool_info.flags = 0;

        VkCommandPool commandpool;
        if(vkCreateCommandPool(logical_device,&commandpool_info,nullptr,&commandpool)!=VK_SUCCESS) throw runtime_error("Error creating command pool");

        VkCommandBufferAllocateInfo allocate_info {};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandPool = commandpool;
        allocate_info.commandBufferCount = swapchain_image_count;

        VkCommandBuffer commandbuffers[swapchain_image_count];
        if(vkAllocateCommandBuffers(logical_device,&allocate_info,commandbuffers)!=VK_SUCCESS) throw runtime_error("Error allocating command buffers"); 

        for(int i = 0; i < swapchain_image_count; ++i){
            VkCommandBufferBeginInfo begin_info {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.pNext = nullptr;
            begin_info.pInheritanceInfo = nullptr;
            begin_info.flags = 0;

            if(vkBeginCommandBuffer(commandbuffers[i],&begin_info)!=VK_SUCCESS) throw runtime_error("Error starting command buffer recording state");

            VkRenderPassBeginInfo renderpass_begin {};
            renderpass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderpass_begin.renderPass = renderpass;
            renderpass_begin.renderArea.offset = {0,0};
            renderpass_begin.renderArea.extent = surface_extent;
            renderpass_begin.pNext = nullptr;
            VkClearValue clear_value {0,0,0,1};
            renderpass_begin.pClearValues = &clear_value;
            renderpass_begin.framebuffer = swapchain_framebuffers[i];
            renderpass_begin.clearValueCount = 1;

            vkCmdBeginRenderPass(commandbuffers[i],&renderpass_begin,VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandbuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
            vkCmdDraw(commandbuffers[i],3,1,0,0);
            vkCmdEndRenderPass(commandbuffers[i]);

            if(vkEndCommandBuffer(commandbuffers[i])!=VK_SUCCESS) throw runtime_error("Error ending command buffer recording state");
        }

        VkSemaphoreCreateInfo semaphore_info {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = nullptr;
        semaphore_info.flags = 0;

        unsigned int frames_in_flight = 2;
        VkSemaphore image_available_semaphore[frames_in_flight],render_finished_semaphore[frames_in_flight];

        for(int i = 0; i < frames_in_flight; ++i) if(vkCreateSemaphore(logical_device,&semaphore_info,nullptr,&image_available_semaphore[i])!=VK_SUCCESS || vkCreateSemaphore(logical_device,&semaphore_info,nullptr,&render_finished_semaphore[i])!=VK_SUCCESS) throw runtime_error("Error creating semaphores");

        unsigned int frame_index = 0;
        VkFenceCreateInfo fence_info {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFence fences[frames_in_flight];
        for(int i = 0; i < frames_in_flight; ++i) if(vkCreateFence(logical_device,&fence_info,nullptr,&fences[i])!=VK_SUCCESS) throw runtime_error("Error creating fence");

        VkQueue render_present_queue;
        vkGetDeviceQueue(logical_device,render_present_queue_index,0,&render_present_queue);
        
        SDL_Event event;
        while(1){
            while(SDL_PollEvent(&event)){
                if(event.type == SDL_QUIT){
                    goto quit;
                    break;
                }
            }

            vkWaitForFences(logical_device,1,&fences[frame_index],VK_TRUE,UINT64_MAX);
            vkResetFences(logical_device,1,&fences[frame_index]);

            unsigned int image_index;
            if(vkAcquireNextImageKHR(logical_device,swapchain,UINT64_MAX,image_available_semaphore[frame_index],VK_NULL_HANDLE,&image_index)!=VK_SUCCESS) throw runtime_error("Error acquiring image");

            VkSubmitInfo submit_info {};
            VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &commandbuffers[image_index];
            submit_info.pNext = nullptr;
            submit_info.pSignalSemaphores = &render_finished_semaphore[frame_index];
            submit_info.pWaitDstStageMask = wait_stages;
            submit_info.pWaitSemaphores = &image_available_semaphore[frame_index];
            submit_info.signalSemaphoreCount = 1;
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.waitSemaphoreCount = 1;

            if(vkQueueSubmit(render_present_queue,1,&submit_info,fences[frame_index])!=VK_SUCCESS) throw runtime_error("Error submitting queue");
        
            VkPresentInfoKHR present_info {};
            present_info.waitSemaphoreCount = 1;
            present_info.swapchainCount = 1;
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.pWaitSemaphores = &render_finished_semaphore[frame_index];
            present_info.pSwapchains = &swapchain;
            present_info.pResults = nullptr;
            present_info.pNext = nullptr;
            present_info.pImageIndices = &image_index;
            frame_index = ++frame_index%2;

            if(vkQueuePresentKHR(render_present_queue,&present_info)!=VK_SUCCESS) throw runtime_error("Error presenting"); 
        }
        quit:;

        vkDeviceWaitIdle(logical_device);

        for(int i = 0; i < frames_in_flight; ++i){
        vkDestroySemaphore(logical_device,image_available_semaphore[i],nullptr);
        vkDestroySemaphore(logical_device,render_finished_semaphore[i],nullptr);
        vkDestroyFence(logical_device,fences[i],nullptr);
        }
        
        vkDestroyCommandPool(logical_device,commandpool,nullptr);
        
        for(int i = 0; i < swapchain_image_count; ++i){
            vkDestroyImageView(logical_device,swapchain_image_views[i],nullptr);
            vkDestroyFramebuffer(logical_device,swapchain_framebuffers[i],nullptr);
        }
        vkDestroySwapchainKHR(logical_device,swapchain,nullptr);        
        vkDestroyPipeline(logical_device,pipeline,nullptr);
        vkDestroyPipelineLayout(logical_device,pipeline_layout,nullptr);
        vkDestroyRenderPass(logical_device,renderpass,nullptr);
        vkDestroyDevice(logical_device,nullptr);
        vkDestroySurfaceKHR(instance,surface,nullptr);
        destroy_debug_messenger(instance,debug_messenger,nullptr);
        vkDestroyInstance(instance,nullptr);

        fclose(debug_file);
    
        SDL_DestroyWindow(window);
        SDL_Quit();
    } catch(const exception &e){
        cerr << e.what() << '\n';
        return -1;
    }
    return 0;
}   