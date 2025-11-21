#ifndef LIBUDEV_SKELETON_H
#define LIBUDEV_SKELETON_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIBUDEV_LOAD_LIBRARY_OK  0
#define LIBUDEV_LOAD_LIBRARY_ERR 1

/**
* udev_list_entry_foreach:
* @list_entry: entry to store the current position
* @first_entry: first entry to start with
*
* Helper to iterate over all entries of a list.
*/
#define udev_list_entry_foreach(list_entry, first_entry) \
        for (list_entry = first_entry; \
             list_entry != NULL; \
             list_entry = udev_list_entry_get_next(list_entry))

// Forward declarations of libudev structures
struct udev;
struct udev_device;
struct udev_enumerate;
struct udev_list_entry;

// Function prototypes
struct udev *udev_new(void);
void udev_unref(struct udev *udev);
struct udev_device *udev_device_new_from_devnum(struct udev *udev, char type, dev_t devnum);
struct udev_device *udev_device_get_parent_with_subsystem_devtype(struct udev_device *dev, const char *subsystem, const char *devtype);
const char *udev_device_get_sysattr_value(struct udev_device *dev, const char *sysattr);
void udev_device_unref(struct udev_device *udev_device);
struct udev_enumerate *udev_enumerate_new(struct udev *udev);
int udev_enumerate_add_match_subsystem(struct udev_enumerate *udev_enumerate, const char *subsystem);
int udev_enumerate_scan_devices(struct udev_enumerate *udev_enumerate);
struct udev_list_entry *udev_enumerate_get_list_entry(struct udev_enumerate *udev_enumerate);
const char *udev_list_entry_get_name(struct udev_list_entry *list_entry);
struct udev_list_entry *udev_list_entry_get_next(struct udev_list_entry *list_entry);
void udev_enumerate_unref(struct udev_enumerate *udev_enumerate);
struct udev_device *udev_device_new_from_syspath(struct udev *udev, const char *syspath);
const char *udev_device_get_devnode(struct udev_device *udev_device);
struct udev_device *udev_device_get_parent(struct udev_device *udev_device);

#ifdef __cplusplus
}
#endif

#endif // LIBUDEV_SKELETON_H