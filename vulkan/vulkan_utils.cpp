#include <array>
#include <iostream>
#include <vector>
#include <stdexcept>
#include "vulkan_utils.h"

PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructure = nullptr;
PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructure = nullptr;
PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemory = nullptr;
PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandle = nullptr;
PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirements = nullptr;
PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructure = nullptr;
PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelines = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandles = nullptr;
PFN_vkCmdTraceRaysNV vkCmdTraceRays = nullptr;

namespace vk {

void load_nv_rtx(VkDevice &device) {
	vkCreateAccelerationStructure =
		reinterpret_cast<PFN_vkCreateAccelerationStructureNV>(vkGetDeviceProcAddr(device,
					"vkCreateAccelerationStructureNV"));
	vkDestroyAccelerationStructure =
		reinterpret_cast<PFN_vkDestroyAccelerationStructureNV>(vkGetDeviceProcAddr(device,
					"vkDestroyAccelerationStructureNV"));
	vkBindAccelerationStructureMemory =
		reinterpret_cast<PFN_vkBindAccelerationStructureMemoryNV>(vkGetDeviceProcAddr(device,
					"vkBindAccelerationStructureMemoryNV"));
	vkGetAccelerationStructureHandle =
		reinterpret_cast<PFN_vkGetAccelerationStructureHandleNV>(vkGetDeviceProcAddr(device,
					"vkGetAccelerationStructureHandleNV"));
	vkGetAccelerationStructureMemoryRequirements =
		reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsNV>(vkGetDeviceProcAddr(device,
					"vkGetAccelerationStructureMemoryRequirementsNV"));
	vkCmdBuildAccelerationStructure =
		reinterpret_cast<PFN_vkCmdBuildAccelerationStructureNV>(vkGetDeviceProcAddr(device,
					"vkCmdBuildAccelerationStructureNV"));
	vkCreateRayTracingPipelines =
		reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(vkGetDeviceProcAddr(device,
					"vkCreateRayTracingPipelinesNV"));
	vkGetRayTracingShaderGroupHandles =
		reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(vkGetDeviceProcAddr(device,
					"vkGetRayTracingShaderGroupHandlesNV"));
	vkCmdTraceRays =
		reinterpret_cast<PFN_vkCmdTraceRaysNV>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysNV"));
}

static const std::array<const char*, 1> validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};

Device::Device() {
	make_instance();
	select_physical_device();
	make_logical_device();

	load_nv_rtx(device);

	// Query the properties we'll use frequently
	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

	rt_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
	VkPhysicalDeviceProperties2 props = {};
	props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props.pNext = &rt_props;
	props.properties = {};
	vkGetPhysicalDeviceProperties2(physical_device, &props);

	std::cout << "Raytracing props:\n"
		<< "max recursion depth: " << rt_props.maxRecursionDepth
		<< "\nSBT handle size: " << rt_props.shaderGroupHandleSize
		<< "\nShader group base align: " << rt_props.shaderGroupBaseAlignment << "\n";
}

Device::~Device() {
	if (instance != VK_NULL_HANDLE) { 
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
	}
}

Device::Device(Device &&d)
	: instance(d.instance), physical_device(d.physical_device), device(d.device), queue(d.queue),
	mem_props(d.mem_props), rt_props(d.rt_props)
{
	d.instance = VK_NULL_HANDLE;
	d.physical_device = VK_NULL_HANDLE;
	d.device = VK_NULL_HANDLE;
	d.queue = VK_NULL_HANDLE;
}

Device& Device::operator=(Device &&d) {
	if (instance != VK_NULL_HANDLE) { 
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
	}
	instance = d.instance;
	physical_device = d.physical_device;
	device = d.device;
	queue = d.queue;
	mem_props = d.mem_props;
	rt_props = d.rt_props;

	d.instance = VK_NULL_HANDLE;
	d.physical_device = VK_NULL_HANDLE;
	d.device = VK_NULL_HANDLE;
	d.queue = VK_NULL_HANDLE;

	return *this;
}

VkDevice Device::logical_device() {
	return device;
}

VkQueue Device::graphics_queue() {
	return queue;
}

VkCommandPool Device::make_command_pool(VkCommandPoolCreateFlagBits flags) {
	VkCommandPool pool = VK_NULL_HANDLE;
	VkCommandPoolCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	create_info.flags = flags;
	create_info.queueFamilyIndex = graphics_queue_index;
	CHECK_VULKAN(vkCreateCommandPool(device, &create_info, nullptr, &pool));
	return pool;
}

uint32_t Device::memory_type_index(uint32_t type_filter, VkMemoryPropertyFlags props) const {
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if (type_filter & (1 << i) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}
	throw std::runtime_error("failed to find appropriate memory");
}

const VkPhysicalDeviceMemoryProperties& Device::memory_properties() const {
	return mem_props;
}

const VkPhysicalDeviceRayTracingPropertiesNV& Device::raytracing_properties() const {
	return rt_props;
}

void Device::make_instance() {
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "rtobj";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "None";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = 0;
	create_info.ppEnabledExtensionNames = nullptr;
	create_info.enabledLayerCount = validation_layers.size();
	create_info.ppEnabledLayerNames = validation_layers.data();

	CHECK_VULKAN(vkCreateInstance(&create_info, nullptr, &instance));
}

void Device::select_physical_device() {
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	std::cout << "Found " << device_count << " devices\n";
	std::vector<VkPhysicalDevice> devices(device_count, VkPhysicalDevice{});
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	for (const auto &d : devices) {
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceProperties(d, &properties);
		vkGetPhysicalDeviceFeatures(d, &features);
		std::cout << properties.deviceName << "\n";

		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(d, nullptr, &extension_count, nullptr);
		std::cout << "num extensions: " << extension_count << "\n";
		std::vector<VkExtensionProperties> extensions(extension_count, VkExtensionProperties{});
		vkEnumerateDeviceExtensionProperties(d, nullptr, &extension_count, extensions.data());
		std::cout << "Device available extensions:\n";
		for (const auto& e : extensions) {
			std::cout << e.extensionName << "\n";
		}

		// Check for RTX support on this device
		auto fnd = std::find_if(extensions.begin(), extensions.end(),
				[](const VkExtensionProperties &e) {
					return std::strcmp(e.extensionName, VK_NV_RAY_TRACING_EXTENSION_NAME) == 0;
				});

		if (fnd != extensions.end()) {
			physical_device = d;
			break;
		}
	}

	if (physical_device == VK_NULL_HANDLE) {
		std::cout << "Failed to find RTX capable GPU\n";
		throw std::runtime_error("Failed to find RTX capable GPU");
	}
}

void Device::make_logical_device() {
	uint32_t num_queue_families = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, nullptr);
	std::vector<VkQueueFamilyProperties> family_props(num_queue_families, VkQueueFamilyProperties{});
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &num_queue_families, family_props.data());
	for (uint32_t i = 0; i < num_queue_families; ++i) {
		if (family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphics_queue_index = i;
			break;
		}
	}

	std::cout << "Graphics queue is " << graphics_queue_index << "\n";
	const float queue_priority = 1.f;

	VkDeviceQueueCreateInfo queue_create_info = {};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = graphics_queue_index;
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkPhysicalDeviceFeatures device_features = {};

	const std::array<const char*, 2> device_extensions = {
		VK_NV_RAY_TRACING_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
	};

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = 1;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.enabledLayerCount = validation_layers.size();
	create_info.ppEnabledLayerNames = validation_layers.data();
	create_info.enabledExtensionCount = device_extensions.size();
	create_info.ppEnabledExtensionNames = device_extensions.data();
	create_info.pEnabledFeatures = &device_features;
	CHECK_VULKAN(vkCreateDevice(physical_device, &create_info, nullptr, &device));
}

VkBufferCreateInfo Buffer::create_info(size_t nbytes, VkBufferUsageFlags usage) {
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = nbytes;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	return info;
}

VkMemoryAllocateInfo Buffer::alloc_info(Device &device, const VkBuffer &buf,
		VkMemoryPropertyFlags mem_props)
{
	VkMemoryRequirements mem_reqs = {};
	vkGetBufferMemoryRequirements(device.logical_device(), buf, &mem_reqs);

	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = mem_reqs.size;
	info.memoryTypeIndex = device.memory_type_index(mem_reqs.memoryTypeBits, mem_props);
	return info;
}

std::shared_ptr<Buffer> Buffer::make_buffer(Device &device, size_t nbytes, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags mem_props)
{
	auto buf = std::make_shared<Buffer>();
	buf->vkdevice = &device;
	buf->buf_size = nbytes;
	buf->host_visible = mem_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	auto create_info = Buffer::create_info(nbytes, usage);
	CHECK_VULKAN(vkCreateBuffer(device.logical_device(), &create_info, nullptr, &buf->buf));

	auto alloc_info = Buffer::alloc_info(device, buf->buf, mem_props);
	CHECK_VULKAN(vkAllocateMemory(device.logical_device(), &alloc_info, nullptr, &buf->mem));

	vkBindBufferMemory(device.logical_device(), buf->buf, buf->mem, 0);

	return buf;
}

Buffer::~Buffer() {
	if (buf != VK_NULL_HANDLE) {
		vkDestroyBuffer(vkdevice->logical_device(), buf, nullptr);
		vkFreeMemory(vkdevice->logical_device(), mem, nullptr);
	}
}

Buffer::Buffer(Buffer &&b)
	: buf_size(b.buf_size), buf(b.buf), mem(b.mem), vkdevice(b.vkdevice), host_visible(b.host_visible)
{
	b.buf_size = 0;
	b.buf = VK_NULL_HANDLE;
	b.mem = VK_NULL_HANDLE;
	b.vkdevice = nullptr;
}

Buffer& Buffer::operator=(Buffer &&b) {
	if (buf != VK_NULL_HANDLE) {
		vkDestroyBuffer(vkdevice->logical_device(), buf, nullptr);
		vkFreeMemory(vkdevice->logical_device(), mem, nullptr);
	}
	buf_size = b.buf_size;
	buf = b.buf;
	mem = b.mem;
	vkdevice = b.vkdevice;
	host_visible = b.host_visible;

	b.buf_size = 0;
	b.buf = VK_NULL_HANDLE;
	b.mem = VK_NULL_HANDLE;
	b.vkdevice = nullptr;
	return *this;
}

std::shared_ptr<Buffer> Buffer::host(Device &device, size_t nbytes, VkBufferUsageFlags usage) {
	return make_buffer(device, nbytes, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

std::shared_ptr<Buffer> Buffer::device(Device &device, size_t nbytes, VkBufferUsageFlags usage) {
	return make_buffer(device, nbytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void* Buffer::map() {
	assert(host_visible);
	void *mapping = nullptr;
	CHECK_VULKAN(vkMapMemory(vkdevice->logical_device(), mem, 0, buf_size, 0, &mapping));
	return mapping;
}

void* Buffer::map(size_t offset, size_t size) {
	assert(host_visible);
	assert(offset + size < buf_size);
	void *mapping = nullptr;
	CHECK_VULKAN(vkMapMemory(vkdevice->logical_device(), mem, offset, size, 0, &mapping));
	return mapping;
}

void Buffer::unmap() {
	assert(host_visible);
	vkUnmapMemory(vkdevice->logical_device(), mem);
}

size_t Buffer::size() const {
	return buf_size;
}

VkBuffer Buffer::handle() const {
	return buf;
}

VkMemoryAllocateInfo Texture2D::alloc_info(Device &device, const VkImage &img) {
	VkMemoryRequirements mem_reqs = {};
	vkGetImageMemoryRequirements(device.logical_device(), img, &mem_reqs);

	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = mem_reqs.size;
	info.memoryTypeIndex = device.memory_type_index(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	return info;
}

Texture2D::~Texture2D() {
	if (image != VK_NULL_HANDLE) {
		vkDestroyImageView(vkdevice->logical_device(), view, nullptr);
		vkDestroyImage(vkdevice->logical_device(), image, nullptr);
		vkFreeMemory(vkdevice->logical_device(), mem, nullptr);
	}
}

Texture2D::Texture2D(Texture2D &&t)
	: tdims(t.tdims), img_format(t.img_format), img_layout(t.img_layout),
	image(t.image), mem(t.mem), view(t.view), vkdevice(t.vkdevice)
{
	t.image = VK_NULL_HANDLE;
	t.mem = VK_NULL_HANDLE;
	t.view = VK_NULL_HANDLE;
	t.vkdevice = nullptr;
}

Texture2D& Texture2D::operator=(Texture2D &&t) {
	if (image != VK_NULL_HANDLE) {
		vkDestroyImageView(vkdevice->logical_device(), view, nullptr);
		vkDestroyImage(vkdevice->logical_device(), image, nullptr);
		vkFreeMemory(vkdevice->logical_device(), mem, nullptr);
	}
	tdims = t.tdims;
	img_format = t.img_format;
	img_layout = t.img_layout;
	image = t.image;
	mem = t.mem;
	view = t.view;
	vkdevice = t.vkdevice;

	t.image = VK_NULL_HANDLE;
	t.view = VK_NULL_HANDLE;
	t.vkdevice = nullptr;
	return *this;
}

std::shared_ptr<Texture2D> Texture2D::device(Device &device, glm::uvec2 dims, VkFormat img_format,
		VkImageUsageFlags usage)
{
	auto texture = std::make_shared<Texture2D>();
	texture->img_format = img_format;
	texture->tdims = dims;
	texture->vkdevice = &device;

	VkImageCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_info.imageType = VK_IMAGE_TYPE_2D;
	create_info.format = texture->img_format;
	create_info.extent.width = texture->tdims.x;
	create_info.extent.height = texture->tdims.y;
	create_info.extent.depth = 1;
	create_info.mipLevels = 1;
	create_info.arrayLayers = 1;
	create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_info.usage = usage;
	create_info.initialLayout = texture->img_layout;
	CHECK_VULKAN(vkCreateImage(device.logical_device(), &create_info, nullptr, &texture->image));

	VkMemoryAllocateInfo alloc_info = Texture2D::alloc_info(device, texture->image);
	CHECK_VULKAN(vkAllocateMemory(device.logical_device(), &alloc_info, nullptr, &texture->mem));

	CHECK_VULKAN(vkBindImageMemory(device.logical_device(), texture->image, texture->mem, 0));

	VkImageViewCreateInfo view_create_info = {};
	view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_create_info.image = texture->image;
	view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_create_info.format = texture->img_format;

	view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_create_info.subresourceRange.baseMipLevel = 0;
	view_create_info.subresourceRange.levelCount = 1;
	view_create_info.subresourceRange.baseArrayLayer = 0;
	view_create_info.subresourceRange.layerCount = 1;

	CHECK_VULKAN(vkCreateImageView(device.logical_device(), &view_create_info, nullptr, &texture->view));
}

size_t Texture2D::pixel_size() const {
	switch (img_format) {
		case VK_FORMAT_B8G8R8A8_UNORM:
			return 4;
		default:
			throw std::runtime_error("Unhandled image format!");
	}
}

VkFormat Texture2D::pixel_format() const {
	return img_format;
}

glm::uvec2 Texture2D::dims() const {
	return tdims;
}

VkImage Texture2D::image_handle() const {
	return image;
}

VkImageView Texture2D::view_handle() const {
	return view;
}

}
