#ifndef _UTILS_H_
#define _UTILS_H_

#define ESP_MEM_CHECK(TAG, a, action)                                                      \
  if (!(a))                                                                                \
  {                                                                                        \
    ESP_LOGE(TAG, "%s:%d (%s): %s", __FILE__, __LINE__, __FUNCTION__, "Memory exhausted"); \
    action;                                                                                \
  }

#endif // _UTILS_H_