#include<linux/module.h>
#include<linux/platform_device.h>
#include "platform.h"

void pcdev_release(struct device *dev)
{
  pr_info("Device released\n");
}


struct pcdev_platform_data pcdev_pdata[]={

	[0] = {.size = 512, .perm =RDWR, .serial_number = "PCDEVABC1111" },
	[1] = {.size = 1024, .perm =RDWR, .serial_number = "PCDEVABC2222" },
	[2] = {.size = 128, .perm =RDONLY, .serial_number = "PCDEVABC3333" },
	[3] = {.size = 32, .perm =WRONLY, .serial_number = "PCDEVABC4444" },
};



struct platform_device platform_pcdev_1 ={
 .name = "pseudo-char-device",
 .id = 0,
 .dev = {
 	.platform_data = &pcdev_pdata[0],
	.release = pcdev_release
 }
};

struct platform_device platform_pcdev_2 = {
.name = "pseudo-char-device",
.id = 1,
.dev = {
        .platform_data = &pcdev_pdata[1],
        .release = pcdev_release
 }

};



static int __init pcdev_platform_init(void)
{
	platform_device_register(&platform_pcdev_1);
	platform_device_register(&platform_pcdev_2);
    pr_info("Device setup module is loaded\n");
	return 0;
}

static void __exit pcdev_platform_exit(void)
{

	platform_device_unregister(&platform_pcdev_1);
    platform_device_unregister(&platform_pcdev_2);
	pr_info("Device setup module is unloaded\n");

}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module which registers platform devices");
MODULE_AUTHOR("Gaurav G Pai");
