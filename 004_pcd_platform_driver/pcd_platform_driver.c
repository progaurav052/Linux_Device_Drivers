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


struct pcdrv_private_data{
	int total_devices;
	dev_t device_num_base; 
	struct class *class_pcd;
	struct device *device_pcd;
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


/*gets called when device is removed from the system */
int pcd_platform_driver_probe(struct platform_device *pdev){

	int ret;

	struct pcdev_private_data *dev_data;

	struct pcdev_platform_data *pdata;

	/*1. Get the Platform data */
	pdata = pdev->dev.platform_data;
	if(!pdata)
	{
		pr_info("No platform data available\n");
		ret = -EINVAL;
		goto out;
	}	

	/*2. if we have the platform data , use that to allocate memory dynamically to device private data*/
	dev_data=kzalloc(sizeof(*dev_data),GFP_KERNEL);
	if(!dev_data){
	  pr_info("Cannot allocate memory \n");
	  ret = -ENOMEM;
	  goto out;
	}

	dev_data->pdata.size = pdata->size;
	dev_data->pdata.perm = pdata->perm;
	dev_data->pdata.serial_number = pdata->serial_number;

	/*3. Dynamically allocate memory to device buffer using size info from platform data which is stored in dev_data in previous step*/
	dev_data->buffer = kzalloc(sizeof(dev_data->pdata.size),GFP_KERNEL);
        if(!dev_data->buffer)
	{
	  pr_info("Cannot allocate memory to the Device buffer\n");
	  return -ENOMEM;
	  goto dev_data_del;
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
		goto buffer_free;
	}

	/*6. Create device file for the Detected Platform device */
	pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,dev_data->dev_num,NULL,"pcdev-%d",pdev->id);
	if(IS_ERR(pcdrv_data.device_pcd))
	{
	 pr_err("Device create failed\n");
	 ret = PTR_ERR(pcdrv_data.device_pcd);
	 goto cdev_del;
	 
	}

	pr_info("A Device is detected\n");
	return 0;

cdev_del:
	cdev_del(&dev_data->pcd_dev);

buffer_free:
	kfree(dev_data->buffer);
dev_data_del:
	kfree(dev_data);

out:
	pr_info("Device probe failed\n");
	return ret;
}

/*gets called when matching happens between platform device and driver*/
int pcd_platform_driver_remove(struct platform_device *pdev){
	pr_info("A Device is removed\n");
        return 0;
}

struct platform_driver pcd_platform_driver ={
 
	.probe = pcd_platform_driver_probe,
	.remove = pcd_platform_driver_remove,
	.driver = {
	    .name = "pseudo-char-device"
	}
};




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

static void __exit pcd_platform_driver_cleanup(void)
{

	platform_driver_unregister(&pcd_platform_driver);
	pr_info("platform driver unloaded\n");

}

module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gaurav G Pai");
MODULE_DESCRIPTION("A Psuedo character platform driver which handles platform devices");

