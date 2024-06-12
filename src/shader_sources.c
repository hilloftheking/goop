const char *COMPUTE_SDF_COMP_SRC = "#version 430\nlayout(local_size_x=4,local_size_y=4,local_size_z=4)in;const vec3 _183[8]=vec3[](vec3(0.5),vec3(0.5,0.5,-0.5),vec3(0.5,-0.5,0.5),vec3(0.5,-0.5,-0.5),vec3(-0.5,0.5,0.5),vec3(-0.5,0.5,-0.5),vec3(-0.5,-0.5,0.5),vec3(-0.5));layout(binding=1,std430)restrict readonly buffer BlobOctree{int blob_ot[];}_146;layout(binding=0,std430)restrict readonly buffer Blobs{vec4 blobs[];}_262;layout(location=0)uniform int blob_count;layout(location=1)uniform vec3 sdf_size;layout(location=2)uniform vec3 sdf_start;layout(binding=0,rgba8)uniform writeonly image3D img_output;vec3 colors[5];float dist_cube(vec3 c,float s,vec3 p){vec3 d=abs(p-c)-vec3(s*0.5);return length(max(d,vec3(0.0)));}float dist_sphere(vec3 c,float r,vec3 p){return length(p-c)-r;}float smin(float a,float b,float k){float h=clamp(0.5+((0.5*(b-a))/k),0.0,1.0);return mix(b,a,h)-((k*h)*(1.0-h));}void main(){colors=vec3[](vec3(0.4099999964237213134765625,0.039999999105930328369140625,0.0599999986588954925537109375),vec3(0.800000011920928955078125,0.800000011920928955078125,0.699999988079071044921875),vec3(0.85000002384185791015625,0.767000019550323486328125,0.1360000073909759521484375),vec3(0.939999997615814208984375,0.56099998950958251953125,0.065800003707408905029296875),vec3(0.1500000059604644775390625,0.085000000894069671630859375,0.0));bool use_octree=blob_count==(-1);vec3 p=((vec3(gl_GlobalInvocationID)+vec3(0.5))*(sdf_size/vec3(128.0)))+sdf_start;int node_idx;int node_leaf_blob_count;if(use_octree){vec3 node_pos=sdf_start+(sdf_size*0.5);float node_size=sdf_size.x;node_idx=0;node_leaf_blob_count=_146.blob_ot[node_idx];while(node_leaf_blob_count==(-1)){bool found_intersection=false;for(int i=0;i<8;i++){vec3 child_pos=node_pos+((vec3(node_size)*_183[i])*0.5);float child_size=node_size*0.5;vec3 param=child_pos;float param_1=child_size;vec3 param_2=p;if(dist_cube(param,param_1,param_2)<=0.0){node_pos=child_pos;node_size=child_size;node_idx=_146.blob_ot[(node_idx+1)+i];node_leaf_blob_count=_146.blob_ot[node_idx];found_intersection=true;break;}}if(!found_intersection){break;}}}float value=1000.0;vec3 color=vec3(0.0);int count;if(use_octree){count=node_leaf_blob_count;}else{count=blob_count;}vec4 blob;for(int i_1=0;i_1<count;i_1++){if(use_octree){int blob_idx=_146.blob_ot[(node_idx+1)+i_1];blob=_262.blobs[blob_idx];}else{blob=_262.blobs[i_1];}int mat_idx=int(blob.w)%5;float radius=float(int(blob.w)/5)/10000.0;vec3 param_3=blob.xyz;float param_4=radius;vec3 param_5=p;float d=dist_sphere(param_3,param_4,param_5);float old_val=value;float param_6=value;float param_7=d;float param_8=0.60000002384185791015625;value=smin(param_6,param_7,param_8);float diff=old_val-value;color=mix(color,colors[mat_idx],vec3(clamp(diff/0.12999999523162841796875,0.0,1.0)));}value=clamp(value,-0.5,1.0);imageStore(img_output,ivec3(gl_GlobalInvocationID),vec4(color,1.0-((value+0.5)/1.5)));}\n";
const char *RAYMARCH_FRAG_SRC = "#version 430\nlayout(location=4)uniform float dist_scale;layout(binding=0)uniform sampler3D sdf_tex;layout(location=0)uniform mat4 model_mat;layout(location=3)uniform vec3 cam_pos;layout(location=0)in vec3 local_pos;layout(location=0)out vec4 out_color;vec2 intersect_aabb(vec3 ro,vec3 rd,vec3 bmin,vec3 bmax){vec3 tmin=(bmin-ro)/rd;vec3 tmax=(bmax-ro)/rd;vec3 t1=min(tmin,tmax);vec3 t2=max(tmin,tmax);float tnear=max(max(t1.x,t1.y),t1.z);float tfar=min(min(t2.x,t2.y),t2.z);return vec2(tnear,tfar);}vec3 get_normal_at(vec3 p){vec3 small_step=vec3(0.20000000298023223876953125*dist_scale,0.0,0.0);vec3 uvw=p+vec3(0.5);float gradient_x=texture(sdf_tex,uvw+small_step.xyy).w-texture(sdf_tex,uvw-small_step.xyy).w;float gradient_y=texture(sdf_tex,uvw+small_step.yxy).w-texture(sdf_tex,uvw-small_step.yxy).w;float gradient_z=texture(sdf_tex,uvw+small_step.yyx).w-texture(sdf_tex,uvw-small_step.yyx).w;return-normalize(vec3(gradient_x,gradient_y,gradient_z));}vec3 ray_march(vec3 ro,vec3 rd){vec3 param=ro;vec3 param_1=rd;vec3 param_2=vec3(-0.5);vec3 param_3=vec3(0.5);vec2 near_far=intersect_aabb(param,param_1,param_2,param_3);if(near_far.y<0.0){return vec3(1.0,0.0,0.0);}float traveled=max(0.0,near_far.x);for(int i=0;i<128;i++){vec3 p=ro+(rd*traveled);vec4 dat=texture(sdf_tex,p+vec3(0.5));float dist=(((1.0-dat.w)*1.5)-0.5)*dist_scale;if(dist<=0.001000000047497451305389404296875){vec3 param_4=p;vec3 normal=get_normal_at(param_4);float light0_val=mix(max(0.0,dot(normal,vec3(-0.3244428336620330810546875,0.811107099056243896484375,-0.4866642653942108154296875))),1.0,0.4000000059604644775390625);float light1_val=mix(max(0.0,dot(normal,vec3(0.1825741827487945556640625,-0.912870943546295166015625,0.365148365497589111328125))),1.0,0.4000000059604644775390625);return((dat.xyz*vec3(1.0,1.0,0.89999997615814208984375))*light0_val)+((dat.xyz*vec3(0.0500000007450580596923828125,0.0500000007450580596923828125,0.07999999821186065673828125))*light1_val);}traveled+=dist;bool _256=traveled>=40.0;bool _264;if(!_256){_264=traveled>=near_far.y;}else{_264=_256;}if(_264){break;}}discard;}void main(){vec3 cam_mdl_pos=(inverse(model_mat)*vec4(cam_pos,1.0)).xyz;vec3 ro=cam_mdl_pos;vec3 rd=normalize(local_pos-ro);vec3 param=ro;vec3 param_1=rd;vec3 _303=ray_march(param,param_1);out_color=vec4(_303,1.0);}\n";
const char *RAYMARCH_VERT_SRC = "#version 430\nlayout(location=2)uniform mat4 proj_mat;layout(location=1)uniform mat4 view_mat;layout(location=0)uniform mat4 model_mat;layout(location=0)out vec3 local_pos;layout(location=0)in vec3 in_pos;void main(){local_pos=in_pos;gl_Position=((proj_mat*view_mat)*model_mat)*vec4(in_pos,1.0);}\n";
