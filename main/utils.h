#ifndef _UTILS_H_
#define _UTILS_H_

#define ESP_MEM_CHECK(TAG, a, action)                                                      \
  if (!(a))                                                                                \
  {                                                                                        \
    ESP_LOGE(TAG, "%s:%d (%s): %s", __FILE__, __LINE__, __FUNCTION__, "Memory exhausted"); \
    action;                                                                                \
  }

#define DEF_INT_EVENT(event) int_##event,
#define DEF_EVENT_EXTERN(event) extern const unsigned long event;
#define DEF_EVENT(event) const unsigned long event = (1UL << int_##event);

#endif // _UTILS_H_