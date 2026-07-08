#include "obs.hpp"
#include "lovense_obs_proc.hpp"
#include "lovense_proc_call.hpp"

#include "browser-proc-handler.hpp"

extern void add_special_effects(lovense::proc::cef_special_effects_callback callback);
extern void remove_special_effects(lovense::proc::cef_special_effects_callback callback);
extern void DispatchJSEvent(std::string eventName, std::string jsonString, int id);
extern void ExecuteOnBrowsersRefresh();

namespace lovense::bs
{	
	void setup_proc_hander()
	{
		proc_handler* handle = obs_get_proc_handler();
		if (handle == nullptr)
		{
			blog(LOG_ERROR, "[lovense-feedback] obs_get_proc_handler failed");
			return;
		}

		proc_handler_add(
			handle,
			"void __lovense_feedback_AddSpecialEffects()",
			[](void* param, calldata_t* data) {
				lovense::proc::cef_special_effects_callback callback{ nullptr };

				calldata_get_ptr(data, "callback", &callback);
				add_special_effects(callback);
			}, nullptr
		);

		proc_handler_add(
			handle,
			"void __lovense_feedback_RemoveSpecialEffects()",
			[](void* param, calldata_t* data) {
				lovense::proc::cef_special_effects_callback callback{ nullptr };

				calldata_get_ptr(data, "callback", &callback);
				remove_special_effects(callback);
			}, nullptr
		);

		proc_handler_add(
			handle,
			"void __lovense_feedback_ForwardJSMessage()",
			[](void* param, calldata_t* data) {

				long long id{ -1 };
				calldata_get_int(data, "id", &id);

				const char* message{ nullptr };
				calldata_get_string(data, "message", &message);

				const char* event_name{ nullptr };
				calldata_get_string(data, "event", &event_name);

				if (message) {
					if (event_name) {
						DispatchJSEvent(event_name, message, id);
					}
					else {
						DispatchJSEvent("lovense_event_notify", message, id);
					}
				}

			}, nullptr);

		proc_handler_add(
			handle,
			"void __lovense_feedback_RefreshDist()",
			[](void* param, calldata_t* data) {

				blog(LOG_INFO, "[lovense feedback]reload dist");
				ExecuteOnBrowsersRefresh();

			}, nullptr);
	}
	
}
