#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/kdev_t.h>
#include<linux/uaccess.h>


#define NO_OF_DEVICES 4

#define MEM_SIZE_MAX_PCDEV1 1024
#define MEM_SIZE_MAX_PCDEV2 512
#define MEM_SIZE_MAX_PCDEV3 1024
#define MEM_SIZE_MAX_PCDEV4 512


#define RDONLY 0x01
#define WRONLY 0x10
#define RDWR   0x11



/*pseudo device's memory */

char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];


struct pcdev_private_data
{
	char *buffer;
	unsigned size;
	const char *serial_number;
	int perm;
	struct cdev pcd_dev;

};

struct pcdrv_private_data
{
	int total_devices;
	dev_t device_number;
	struct class *class_pcd;
	struct device *device_pcd;
	struct pcdev_private_data pcdev_data[NO_OF_DEVICES];

};


struct pcdrv_private_data pcdrv_data={
   	.total_devices = NO_OF_DEVICES,
	.pcdev_data={
	  [0]={
	     .buffer = device_buffer_pcdev1,
	     .size = MEM_SIZE_MAX_PCDEV1,
	     .serial_number = "DEV123XYZ",
	     .perm = RDONLY
	  },

	  [1]={
             .buffer = device_buffer_pcdev2,
             .size = MEM_SIZE_MAX_PCDEV2,
             .serial_number = "DEV223XYZ",
             .perm = WRONLY
          },

	  [2]={
             .buffer = device_buffer_pcdev3,
             .size = MEM_SIZE_MAX_PCDEV3,
             .serial_number = "DEV323XYZ",
             .perm = RDWR
          },

	  [3]={
             .buffer = device_buffer_pcdev4,
             .size = MEM_SIZE_MAX_PCDEV4,
             .serial_number = "DEV423XYZ",
             .perm = RDWR
          }


	}
};

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

        int max_size=pcdev_data->size;

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

	int max_size=pcdev_data->size;

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

        int max_size=pcdev_data->size;


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
	ret = check_permission(pcdev_data->perm,filp->f_mode);

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
static int __init pcd_driver_init(void)
{

 int ret;
 int i;
 /*1. Dynamically allocate a device number */
 ret = alloc_chrdev_region(&pcdrv_data.device_number,0,4,"pcd_devices");
 if(ret < 0)
 {
	 pr_err("Alloc chrdev failed\n");
	 goto out;
 }

 /*4. create device class under /sys/class/ */
 pcdrv_data.class_pcd = class_create(THIS_MODULE,"pcd_class");

 if(IS_ERR(pcdrv_data.class_pcd))
 {
         pr_err("class Creation failed");
         ret = PTR_ERR(pcdrv_data.class_pcd);
         goto unreg_chrdev;
 }

 for(i=0;i<NO_OF_DEVICES;i++)
 {

 pr_info("The Device number <Major>:<Minor> = %d:%d\n",MAJOR(pcdrv_data.device_number+i),MINOR(pcdrv_data.device_number+i));

 /*2. Initialize the cdev structure with fops */
 cdev_init(&pcdrv_data.pcdev_data[i].pcd_dev,&pcd_fops);

 /*3. Register a device (cdev structure) with VFS */
 pcdrv_data.pcdev_data[i].pcd_dev.owner= THIS_MODULE;
 ret = cdev_add(&pcdrv_data.pcdev_data[i].pcd_dev,pcdrv_data.device_number+i,1);
 if(ret <0 )
 {
 	pr_err("Cdev add failed\n");
	goto cdev_del;
 }

 /*5. populate the sysfd with device information */
 pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd,NULL,pcdrv_data.device_number+i,NULL,"pcdev-%d",i+1);

  if(IS_ERR(pcdrv_data.device_pcd))
  {
         pr_err("Device Creation failed");
         ret = PTR_ERR(pcdrv_data.device_pcd);
         goto class_del;
  }

 }

 pr_info("The module init was successfull\n");
 return 0;

 cdev_del:
 class_del:
	for(;i>=0;i--)
	{
		device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_number + i);
		cdev_del(&pcdrv_data.pcdev_data[i].pcd_dev);

	}
	class_destroy(pcdrv_data.class_pcd);

 unreg_chrdev:
	unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);

 out:
 	pr_info("Module insertion failed\n");
	return ret;


}

static void __exit pcd_driver_cleanup(void)
{
        int i;
	for(i=0;i<NO_OF_DEVICES;i++)
        {
                device_destroy(pcdrv_data.class_pcd,pcdrv_data.device_number + i);
                cdev_del(&pcdrv_data.pcdev_data[i].pcd_dev);

        }
        class_destroy(pcdrv_data.class_pcd);
        unregister_chrdev_region(pcdrv_data.device_number,NO_OF_DEVICES);


}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GAURAV G PAI");
MODULE_DESCRIPTION(" A Psuedo character device driver");

