//Heightmap Normalizing (range 0-255)
void bmap_normalize(BMAP* bmap_)
{
	var size_x_=bmap_width(bmap_);
	var size_y_=bmap_height(bmap_);
	
	var pmax=0;
	
	int py; for(py=0;py<size_y_;py++)
	{
		int px; for(px=0;px<size_x_;px++)
		{
			COLOR color_;
			var format=bmap_lock(bmap_,0);var alpha_;
			var pixel=pixel_for_bmap(bmap_, px, py);
			pixel_to_vec(color_,alpha_,format,pixel);
			bmap_unlock(bmap_);
			
			//find brightest pixel to prepare normalizing
			if(color_.red>pmax){pmax=color_.red;}
		}
	}

	//normalize heightmap (range 0-255)
	var factor=255/pmax;
	int py; for(py=0;py<size_y_;py++)
	{
		int px; for(px=0;px<size_x_;px++)
		{
			COLOR color_;
			var format=bmap_lock(bmap_,0);var alpha_;
			var pixel=pixel_for_bmap(bmap_, px, py);
			pixel_to_vec(color_,alpha_,format,pixel);
			bmap_unlock(bmap_);
			
			color_.red*=factor;
			color_.green*=factor;
			color_.blue*=factor;
			
			color_.red=clamp(integer(color_.red),0,255);
			color_.green=clamp(integer(color_.green),0,255);
			color_.blue=clamp(integer(color_.blue),0,255);
			
			var format=bmap_lock(bmap_,0);
			var pixel=pixel_for_vec(color_,100,format);
			pixel_to_bmap(bmap_, px, py, pixel);
			bmap_unlock(bmap_);
		}
	}
}

//Thermal Erosion Filter
//recommended default values:
//iterations_ 		= 50
//threshold_		= 3
//coefficient_		= 0.5
var* thermal_erosion_tmp=NULL;
///////////////////////////////
void bmap_thermal_erosion_filter(BMAP* bmap_,var iterations_,var threshold_,var coefficient_)
{
	//normalize heightmap
	bmap_normalize(bmap_);
	
	//get size
	double size_x_=bmap_width(bmap_);
	double size_y_=bmap_height(bmap_);
	
	//create data
	thermal_erosion_tmp=(var*)sys_malloc(sizeof(var) * size_x_*size_y_);
	
	//fill data with pixel color
	int py; for(py=0;py<size_y_;py++)
	{
		int px; for(px=0;px<size_x_;px++)
		{
			COLOR color_;
			var format=bmap_lock(bmap_,0);var alpha_;
			var pixel=pixel_for_bmap(bmap_, px, py);
			pixel_to_vec(color_,alpha_,format,pixel);
			bmap_unlock(bmap_);
			
			thermal_erosion_tmp[px*size_x_+py]=color_.red;
		}
	}
	
	int i; for(i=0;i<iterations_;i++)
	{
		int py; for(py=0;py<size_y_;py++)
		{
			int px; for(px=0;px<size_x_;px++)
			{
				var h_=thermal_erosion_tmp[px*size_x_+py];
				
				var d[4]; COLOR color_tmp_; var h[4];
				
				h[0]=thermal_erosion_tmp[clamp(px-1,0,size_x_-1)*size_x_+clamp(py,0,size_y_-1)];
				d[0]=h_-h[0];

				h[1]=thermal_erosion_tmp[clamp(px,0,size_x_-1)*size_x_+clamp(py-1,0,size_y_-1)];
				d[1]=h_-h[1];

				h[2]=thermal_erosion_tmp[clamp(px+1,0,size_x_-1)*size_x_+clamp(py,0,size_y_-1)];
				d[2]=h_-h[2];

				h[3]=thermal_erosion_tmp[clamp(px,0,size_x_-1)*size_x_+clamp(py+1,0,size_y_-1)];
				d[3]=h_-h[3];

				var dtotal=0,dmax=-9999;
				int j; for(j=0;j<4;j++)
				{
					if(d[j]>threshold_)
					{
						dtotal=dtotal+d[j];
						if(d[j]>dmax)
						{
							dmax=d[j];
						}
					}
				}
				
				int j; for(j=0;j<4;j++)
				{
					if(dtotal!=0)
					{
						h[j]=h[j] + coefficient_*(dmax - threshold_) * (d[j]/dtotal);
					}
				}
				
				thermal_erosion_tmp[clamp(px-1,0,size_x_-1)*size_x_+clamp(py,0,size_y_-1)]=h[0];
				thermal_erosion_tmp[clamp(px,0,size_x_-1)*size_x_+clamp(py-1,0,size_y_-1)]=h[1];
				thermal_erosion_tmp[clamp(px+1,0,size_x_-1)*size_x_+clamp(py,0,size_y_-1)]=h[2];
				thermal_erosion_tmp[clamp(px,0,size_x_-1)*size_x_+clamp(py+1,0,size_y_-1)]=h[3];
			}
		}
	}
	
	//fill pixels with array data
	int py; for(py=0;py<size_y_;py++)
	{
		int px; for(px=0;px<size_x_;px++)
		{
			//set colors
			COLOR bmp_color_;
			bmp_color_.red=thermal_erosion_tmp[px*size_x_+py];
			bmp_color_.green=thermal_erosion_tmp[px*size_x_+py];
			bmp_color_.blue=thermal_erosion_tmp[px*size_x_+py];
			
			//add to heightmap
			var format=bmap_lock(bmap_,0);
			var pixel=pixel_for_vec(bmp_color_,100,format);
			pixel_to_bmap(bmap_, px, py, pixel);
			bmap_unlock(bmap_);
		}
	}
	
	//free data
	sys_free(thermal_erosion_tmp);
}