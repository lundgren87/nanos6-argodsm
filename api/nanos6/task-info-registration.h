#ifndef NANOS6_TASK_INFO_REGISTRATION_H
#define NANOS6_TASK_INFO_REGISTRATION_H

enum nanos6_task_info_registration_api_t { nanos6_task_info_registration_api = 1 };


#include "task-instantiation.h"

#ifdef __cplusplus
extern "C" {
#endif


//! \brief Register a type of task
//! 
//! \param[in] task_info a pointer to the nanos_task_info structure
void nanos_register_task_info(nanos_task_info *task_info);


#ifdef __cplusplus
}
#endif


#endif /* NANOS6_TASK_INFO_REGISTRATION_H */