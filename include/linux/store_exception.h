/*< DTS2014122907589 mazhenhua 20141229 begin */
/* < DTS2014082110037 shenchen 20140821 begin */
#ifndef __LINUX_STOREEXCEPTION_H
#define __LINUX_STOREEXCEPTION_H

/**
 *  name: the name of this command
 *  msg: concrete command string to write to /dev/log/exception
 *  return: on success return the bytes writed successfully, on error return -1
 *
*/
int store_exception(char* name, char* msg);


#endif
/* DTS2014082110037 shenchen 20140821 end> */
/* DTS2014122907589 mazhenhua 20141229 end >*/
