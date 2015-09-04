#include "imx219_otp.h"

#define MSM_EEPROM_DEBUG
#undef CDBG
#ifdef MSM_EEPROM_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define OTP_DRV_LSC_SIZE 70   
#define OTP_DRV_START_ADDR 0x3204
#define OTP_DRV_END_ADDR 0x3243
#define OTP_DRV_FLAG_ADDR 0x3204


  int imx219_eeprom_parse_memory_map(struct device_node *of,
				       struct msm_eeprom_memory_block_t *data)
{
	int i, rc = 0;
	char property[PROPERTY_MAXSIZE];
	uint32_t count = 6;
	struct msm_eeprom_memory_map_t *map;

	snprintf(property, PROPERTY_MAXSIZE, "qcom,num-blocks");
	rc = of_property_read_u32(of, property, &data->num_map);
	CDBG("%s: %s %d\n", __func__, property, data->num_map);
	if (rc < 0) {
		pr_err("%s failed rc %d\n", __func__, rc);
		return rc;
	}

	map = kzalloc((sizeof(*map) * data->num_map), GFP_KERNEL);
	if (!map) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	data->map = map;

	for (i = 0; i < data->num_map; i++) {
		snprintf(property, PROPERTY_MAXSIZE, "qcom,init%d", i);
		rc = of_property_read_u32_array(of, property,
				(uint32_t *) &map[i].init, count);
		if (rc < 0) {
			pr_err("%s: there is no init parameters %d\n", __func__, __LINE__);
			
		}
		snprintf(property, PROPERTY_MAXSIZE, "qcom,page%d", i);
		rc = of_property_read_u32_array(of, property,
				(uint32_t *) &map[i].page, count);
		if (rc < 0) {
			pr_err("%s: failed %d\n", __func__, __LINE__);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE,
					"qcom,pageen%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].pageen, count);
		if (rc < 0)
			pr_err("%s: pageen not needed\n", __func__);

		snprintf(property, PROPERTY_MAXSIZE, "qcom,saddr%d", i);
		rc = of_property_read_u32_array(of, property,
			(uint32_t *) &map[i].saddr.addr, 1);
		if (rc < 0)
			CDBG("%s: saddr not needed - block %d\n", __func__, i);

		snprintf(property, PROPERTY_MAXSIZE, "qcom,poll%d", i);
		rc = of_property_read_u32_array(of, property,
				(uint32_t *) &map[i].poll, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE, "qcom,mem%d", i);
		rc = of_property_read_u32_array(of, property,
				(uint32_t *) &map[i].mem, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		data->num_data += map[i].mem.valid_size;
	}

	CDBG("%s num_bytes %d\n", __func__, data->num_data);
	data->num_data = 512;
	CDBG("%s assume data lenth  %d\n", __func__, data->num_data);
	data->mapdata = kzalloc(data->num_data, GFP_KERNEL);
	if (!data->mapdata) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR;
	}
	return rc;

ERROR:
	kfree(data->map);
	memset(data, 0, sizeof(*data));
	return rc;
}

static int IMX219_write_i2c(struct msm_eeprom_ctrl_t *e_ctrl, uint32_t addr, uint32_t data)
{
	int rc;
	e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_write(&(e_ctrl->i2c_client), addr,data, MSM_CAMERA_I2C_BYTE_DATA);
	return rc;
}
#if 1
static int IMX219_read_i2c(struct msm_eeprom_ctrl_t *e_ctrl, uint32_t addr)
{
	int rc;
	uint16_t data;
	e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_read(&(e_ctrl->i2c_client), addr,&data, MSM_CAMERA_I2C_BYTE_DATA);
	return (int)data;
}
#endif
#if 1
static int IMX219_read_i2c_seq(struct msm_eeprom_ctrl_t *e_ctrl, uint32_t addr, uint8_t *data,uint32_t length)
{
	int rc;
	if(data ==NULL) return -1;
	e_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;

	rc = e_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(&(e_ctrl->i2c_client), addr, data,length);
	return rc;
}
#endif
static int IMX219_readPage(struct msm_eeprom_ctrl_t *e_ctrl, int page, unsigned char *out_buf)
{
 int cnt;
 int flag;
 int temp;
 flag = 0 ;
 CDBG("linlukun    E %s  page =%d\n", __func__,page);

 if(page<0 || page>11 ){
  CDBG("linlukun    [page =%d] %s\n", page, __func__);
  return -1;
 }
 if(out_buf==NULL) return -1;
 IMX219_write_i2c(e_ctrl, 0x0100,0x00);
 
 IMX219_write_i2c(e_ctrl, 0x3302,0x01);
 IMX219_write_i2c(e_ctrl, 0x3303,0x2c);
 IMX219_write_i2c(e_ctrl, 0x012A,0x0c);
 IMX219_write_i2c(e_ctrl, 0x012B,0x00);
 
 IMX219_write_i2c(e_ctrl, 0x3300,0x08);
 
 IMX219_write_i2c(e_ctrl, 0x3200,0x01);
 
 cnt = 0;

 while((IMX219_read_i2c(e_ctrl, 0x3201) & 0x01)==0)
 {
   cnt++;
   msleep(10);
   if(cnt==5) break;
 }
 
 IMX219_write_i2c(e_ctrl, 0x3202, page);
 CDBG("linlukun    now in [page =%d] %s\n", page, __func__);
 
 flag = (unsigned char)IMX219_read_i2c(e_ctrl, OTP_DRV_FLAG_ADDR);
 CDBG("linlukun    now in [page =%d] %s flag = %x\n", page, __func__,flag);
 if(page == 0 || page == 5)
 {
	if(flag == 0)
	{
	   return 0;
	}
  CDBG(KERN_ERR"--------------------------------------\n");
  temp = IMX219_read_i2c(e_ctrl, 0x3205); 
  CDBG(KERN_ERR"linlukun    module_integrator_id : %d\n",temp);
  temp = IMX219_read_i2c(e_ctrl, 0x320A); 
  CDBG(KERN_ERR"linlukun    lens_id : %d\n",temp);
  temp = IMX219_read_i2c(e_ctrl, 0x3207); 
  CDBG(KERN_ERR"linlukun    production_year : %d\n",temp);
  temp = IMX219_read_i2c(e_ctrl, 0x3208); 
  CDBG(KERN_ERR"linlukun    production_month : %d\n",temp);
  temp = IMX219_read_i2c(e_ctrl, 0x3209); 
  CDBG(KERN_ERR"linlukun    production_day : %d\n",temp);
  CDBG(KERN_ERR"--------------------------------------\n");  
 } 
 
 
 #if 0
 for(myreg=OTP_DRV_START_ADDR;  myreg <= OTP_DRV_END_ADDR;  myreg++)
 {
  out_buf[myreg-0x3204]=(unsigned char)IMX219_read_i2c(e_ctrl, (myreg));
 }
 #endif
 IMX219_read_i2c_seq(e_ctrl,OTP_DRV_START_ADDR,&out_buf[OTP_DRV_START_ADDR-0x3204],(OTP_DRV_END_ADDR-OTP_DRV_START_ADDR+1));
 
 cnt=0;
 while((IMX219_read_i2c(e_ctrl, 0x3201) & 0x01)==0)
 {
  cnt++;
  msleep(10);
  if(cnt==5) break;
 }
  CDBG( "linlukun    X %s\n", __func__);
 return 1;
}



static int IMX219_ReadLSC(struct msm_eeprom_ctrl_t *e_ctrl,
			      struct msm_eeprom_memory_block_t *block)
{

	 uint8_t *tempData = block->mapdata;
	 int i ,rc, group;
	 unsigned int sum;
	 CDBG("linlukun    E %s\n", __func__); 

	 for(group = 2; group >0; group--){
	  for(i = 0; i < 5; i++){
	   rc = IMX219_readPage(e_ctrl,(5*(group -1) + i),tempData+i*64);
	   if(rc == 0 && (i+(group-1)*5)%5 == 0){
	    CDBG("otp info is not in group %d\n", group);
	    break;
	   }
	  }
	 }
	  
	 sum = 0;
	 for (i = 1; i < 16+280; i++ )
	 {
	  sum += tempData[i];
	  sum = sum%0xff;
	 }
	 CDBG("linlukun sum=%d\n", sum);
	 sum = sum%0xff;
	 CDBG("linlukun sum=%d,   tempData[296]=%d\n", (int)sum , tempData[296]);
	 if( (sum+1) != tempData[296] )
	 {
	  printk("otp info checksum error !\n");
	  return -1;
	 }
	return 0;
}


 int imx219_read_eeprom_memory(struct msm_eeprom_ctrl_t *e_ctrl,
			      struct msm_eeprom_memory_block_t *block)
 {
	int rc = 0;

	if (!e_ctrl) {
		pr_err("%s e_ctrl is NULL", __func__);
		return -EINVAL;
	}

	rc = IMX219_ReadLSC(e_ctrl,block);
	return rc;
	
}
