#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>
#include<linux/platform_device.h>
#include<linux/slab.h>
#include<linux/mod_devicetable.h>
#include "platform.h"


#define MAX_DEVICES 10

struct pcdev_private_data{
 	struct pcdev_platform_data pdata;
 	char *buffer;
	dev_t dev_num;
	struct cdev pcd_dev;
};
/*we wull dynmically create this structure every time probe runs (match happens), this will also denote each device , pcd_dev will be added to VFS */


struct pcdrv_private_data{
	int total_devices;
	dev_t device_num_base; 
	struct class *class_pcd;
	struct device *device_pcd;
};
/*to keep track fo devices and pointers needed during class and device create , also tracks base device number*/


enum pcdev_names
{
	PCDEVA1X,
	PCDEVB1X,
	PCDEVC1X,
	PCDEVD1X
};

struct device_config 
{
	int config_item1;
	int config_item2;
};


/*configuration data of the driver for devices */
struct device_config pcdev_config[] = 
{
	[PCDEVA1X] = {.config_item1 = 60, .config_item2 = 21},
	[PCDEVB1X] = {.config_item1 = 50, .config_item2 = 22},
	[PCDEVC1X] = {.config_item1 = 40, .config_item2 = 23},
	[PCDEVD1X] = {.config_item1 = 30, .config_item2 = 24}
	
};

struct pcdrv_private_data pcdrv_data;

int check_permission(int dev_perm , int acc_mode)
{
  if(dev_perm == RDWR)
  {
          return 0;
  }
  if((dev_perm == RDONLY) && ((acc_mode & FMODE_READ)  && !(acc_mode & FMODE_WRITE)))
  {
    return 0;
  }
  if((dev_perm == WRONLY) && ((acc_mode & FMODE_WRITE)  && !(acc_mode & FMODE_READ)))
  {
    return 0;
  }

  return -EPERM;


}


loff_t pcd_llseek (struct file *filp, loff_t offset, int whence){

        struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)filp->private_data;

        int max_size=pcdev_data->pdata.size;

        loff_t temp;


        pr_info("lseek requested \n");
        pr_info("current value of the file position= %lld\n",filp->f_pos);

        switch(whence)
        {
                case SEEK_SET:
                        if((offset > max_size) || (offset <0))
                                return -EINVAL;
                        filp->f_pos = offset;
                        break;
                case SEEK_CUR:
                        temp = filp->f_pos + offset;
                        if((temp > max_size) || (temp <0))
                                return -EINVAL;
                        filp->f_pos = temp;
                        break;
                case SEEK_END:
                        temp = max_size + offset;
                        if((temp > max_size) || (temp <0))
                                return -EINVAL;
                        filp->f_pos = temp;
                        break;
                default:
                        return -EINVAL;
                                                                                   

       }

        pr_info("New Value of the file position = %lld\n",filp->f_pos);
        return filp->f_pos;

}


ssize_t pcd_read (struct file *filp, char __user *buff, size_t count, loff_t *f_pos){

        struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)filp->private_data;

        int max_size=pcdev_data->pdata.size;

        pr_info("read requested for %zu bytes \n",count);
        pr_info("Current file position =%lld\n",*f_pos);


        /* Adjust the 'count' */
        if((*f_pos + count) > max_size)
        {
           count = max_size - *f_pos;
        }

        if(!count)
        {
                pr_err("No More space left on the Device to Read - Reached EOF\n");
                return -ENOMEM;
        }

        /* copy to user */
        if(copy_to_user(buff,pcdev_data->buffer+(*f_pos),count)){
         return -EFAULT;
        }

        /*update the current file position */
        *f_pos += count;

        pr_info("Number of bytes successfully read=%zu\n",count);
        pr_info("Updated file position =%lld\n",*f_pos);

                /*return the number of bytes which have been successfully read */
        return count;

}

ssize_t pcd_write (struct file *filp, const char __user *buff, size_t count, loff_t *f_pos){


        struct pcdev_private_data *pcdev_data = (struct pcdev_private_data*)filp->private_data;

        int max_size=pcdev_data->pdata.size;


        pr_info("write requested for %zu bytes \n",count);
        pr_info("Current file position =%lld\n",*f_pos);


        /* Adjust the 'count' */
        if((*f_pos + count) > max_size)
        {
           count = max_size - *f_pos;
        }

        /* this is important denoting nothing can be written, as no memory */
        if(!count){
        pr_err("No More space left on the Device to Write");
        return -ENOMEM;
        }

        /* copy to user */
        if(copy_from_user(pcdev_data->buffer+(*f_pos),buff,count)){
         /* return 0 on success and if not returns no of bytes still left to be copied */
         return -EFAULT;
	}

        /*update the current file position */
        *f_pos += count;

        pr_info("Number of bytes successfully written=%zu\n",count);
        pr_info("Updated file position =%lld\n",*f_pos);

        /*return the number of bytes which have been successfully wriiten */
        return count;

}

int pcd_open (struct inode *inode, struct file *filp){

        int ret;

        int minor_n;

        struct pcdev_private_data *pcdev_data;

        /*find out on which device file open was attempted by the user space */

        minor_n = MINOR(inode->i_rdev);
        pr_info("minor access = %d\n",minor_n);

        /*get the devices private data structure */
        pcdev_data = container_of(inode->i_cdev,struct pcdev_private_data,pcd_dev);

        /* save the above address , to supply device private data to other methods of the driver */
        filp->private_data = pcdev_data;

        /*check permission */
        ret = check_permission(pcdev_data->pdata.perm,filp->f_mode);

                (!ret)?pr_info("open was successfull\n"):pr_info("open was unsuccessfull\n");

        return ret;

}

int pcd_release (struct inode *inode, struct file *filp){

        pr_info("Release operation Successfull\n");
        return 0;

}


/* file operations of the driver */
struct file_operations pcd_fops={
        .open=pcd_open,
        .write = pcd_write,
        .read= pcd_read,
        .llseek = pcd_llseek,
        .release = pcd_release,
        .owner = THIS_MODULE
};

/*gets called when matching happens between platform device and driver*/
int pcd_platform_driver_probe(struct platform_device *pdev){

	int ret;

        /*Pointer to device private data structure which is created everytime probe is called for every platform Device*/
	struct pcdev_private_data *dev_data;

        /*Pointer to platform Data , coming from platform.h*/
	struct pcdev_platform_data *pdata;

        pr_info("A Device is detected\n");
	/*1. Get the Platform data */
	pdata = pdev->dev.platform_data;
	if(!pdata)
	{
		pr_info("No platform data available\n");
		ret = -EINVAL;
		return ret;
	}	

	/*2. if we have the platform data , use that to allocate memory dynamically to device private data*/
	dev_data=devm_kmalloc(&pdev->dev,sizeof(*dev_data),GFP_KERNEL);
	if(!dev_data){
	  pr_info("Cannot allocate memory \n");
	  ret = -ENOMEM;
          return ret;
	  
	}

        pdev->dev.driver_data = dev_data; /* for future use, we can set the device private data using pdev->dev.driver_data*/

	dev_data->pdata.size = pdata->size;
	dev_data->pdata.perm = pdata->perm;
	dev_data->pdata.serial_number = pdata->serial_number;

        pr_info("Device serial number = %s\n",dev_data->pdata.serial_number);
	pr_info("Device size = %d\n", dev_data->pdata.size);
	pr_info("Device permission = %d\n",dev_data->pdata.perm);

        pr_info("Config item 1 = %d\n",pcdev_config[pdev->id_entry->driver_data].config_item1 );
	pr_info("Config item 2 = %d\n",pcdev_config[pdev->id_entry->driver_data].config_item2 );


	/*3. Dynamically allocate memory to device buffer using size info from platform data which is stored in dev_data in previous step*/
	dev_data->buffer = devm_kmalloc(&pdev->dev,sizeof(dev_data->pdata.size),GFP_KERNEL);
        if(!dev_data->buffer)
	{
	  pr_info("Cannot allocate memory to the Device buffer\n");
	  ret = -ENOMEM;
	  return ret;
	}

	/*4. Get the device number, we know the Base Device Number add this to Dev id for each platform Device  */
	dev_data->dev_num = pcdrv_data.device_num_base + pdev->id;

	/*5 Do Cdev init and cdev add */
	cdev_init(&dev_data->pcd_dev,&pcd_fops);

	dev_data->pcd_dev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->pcd_dev,dev_data->dev_num,1);
	if(ret < 0)
	{
		pr_err("Cdev add failed\n");
		return ret;
	}

	/*6. Create device file for the Detected Platform device */
	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,dev_data->dev_num,NULL,"pcdev-%d",pdev->id);
	if(IS_ERR(pcdrv_data.device_pcd))
	{
	 pr_err("Device create failed\n");
	 ret = PTR_ERR(pcdrv_data.device_pcd);
	 cdev_del(&dev_data->pcd_dev); /*removes the cdev from VFS*/
         return ret;
	 
	}

        pcdrv_data.total_devices++;
	
	return 0;


}


/*gets called when device is removed from the system */
int pcd_platform_driver_remove(struct platform_device *pdev){
        struct pcdev_private_data *dev_data = pdev->dev.driver_data;
        /*Called when for matching paltform device and driver link is removed --> happens when device is removed or driver is removed*/
        /*1. Remove the Device that was created with device create()*/
        device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_num_base + pdev->id);
        /*2. Reomve the cdev entry from the kernel VFS*/
        cdev_del(&dev_data->pcd_dev);
        //since i am allocating memory using devm version which is resource managed, i dont need to free the memory explicitly, it will be freed when device is removed
        /*3. Free the memory held by the Device (like dev_data and dev_data->buffer that we created in probe) 
        kfree(dev_data->buffer);
        kfree(dev_data);*/
        
        pcdrv_data.total_devices--;

	pr_info("A Device is removed\n");
        return 0;
}

struct platform_device_id pcd_platform_ids[] = {
        {.name = "pcdev-A1x", .driver_data = PCDEVA1X},
        {.name = "pcdev-B1x", .driver_data = PCDEVB1X},
        {.name = "pcdev-C1x", .driver_data = PCDEVC1X,},
        {.name = "pcdev-D1x", .driver_data = PCDEVD1X},
        { /*sentinel*/ } /* should be last entry, to indicate end of the array , NULL entry is used to indicate end of the array*/
};

/* Created for platform driver registration and unregistration*/
struct platform_driver pcd_platform_driver ={
 
	.probe = pcd_platform_driver_probe,
	.remove = pcd_platform_driver_remove,
        .id_table = pcd_platform_ids, /*when we use this field , "name" of "driver" struct   will not be used for matching */
	.driver = { /*struct device_driver driver;*/ 
	    .name = "pseudo-char-device"
	}
};

/*Called when module is loaded*/
static int __init pcd_platform_driver_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&pcdrv_data.device_num_base,0,MAX_DEVICES,"pcdevs");
	if(ret < 0)
	{
		pr_err("Alloc chrdev failed\n");
		return ret;
	}

	pcdrv_data.class_pcd = class_create(THIS_MODULE,"pcd_class");
	if(IS_ERR(pcdrv_data.class_pcd)){
		 pr_err("class creation failed\n");
		 ret = PTR_ERR(pcdrv_data.class_pcd);
		 unregister_chrdev_region(pcdrv_data.device_num_base,MAX_DEVICES);
		 return ret;
	}

	platform_driver_register(&pcd_platform_driver);

	pr_info("platform driver loaded\n");

	return 0;
}

/*Called when module is unloaded*/
static void __exit pcd_platform_driver_cleanup(void)
{

	platform_driver_unregister(&pcd_platform_driver); 
        /*here we can delete the class, because at this point the remove function will be executed for all the Matching Devices (device files , cdev will be destroyed ) */
        class_destroy(pcdrv_data.class_pcd);
        unregister_chrdev_region(pcdrv_data.device_num_base,MAX_DEVICES);       
	pr_info("platform driver unloaded\n");

}

/*Registering entry point of init and exit */
module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gaurav G Pai");
MODULE_DESCRIPTION("A Psuedo character platform driver which handles platform devices");

