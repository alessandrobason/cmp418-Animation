#include "input_manager_d3d11.h"
#include "touch_input_manager_d3d11.h"
#include "keyboard_d3d11.h"
#include "sony_controller_input_manager_d3d11.h"
#include <platform/d3d11/system/platform_d3d11.h>

#include <system/allocator.h>

namespace gef
{
	InputManager* InputManager::Create(Platform& platform, IAllocator *alloc)
	{
		return alloc->make<InputManagerD3D11>(platform);
	}

	InputManagerD3D11::InputManagerD3D11(Platform& platform)
		: InputManager(platform)
		, platform_(platform)
		, direct_input_(NULL)
	{
		HRESULT hresult = S_OK;
		// Setup DirectInput
		hresult = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&direct_input_, NULL);

		if (hresult == S_OK)
		{
			PlatformD3D11& platform_d3d11 = reinterpret_cast<PlatformD3D11&>(platform);

			touch_manager_ = g_alloc->make<TouchInputManagerD3D11>(&platform_d3d11, direct_input_);
			platform.set_touch_input_manager(touch_manager_);
			keyboard_ = g_alloc->make<KeyboardD3D11>(platform_d3d11, direct_input_);
			controller_manager_ = g_alloc->make<SonyControllerInputManagerD3D11>(platform_d3d11, direct_input_);
		}
	}

	InputManagerD3D11::~InputManagerD3D11()
	{
		g_alloc->destroy(touch_manager_);
		platform_.set_touch_input_manager(NULL);

		g_alloc->destroy(keyboard_);
		g_alloc->destroy(controller_manager_);

		if (direct_input_)
			direct_input_->Release();
	}

}