const char *COMPUTE_SDF_COMP_SRC = "#version 430\nlayout(local_size_x=4,local_size_y=4,local_size_z=4)in;const vec3 _188[8]=vec3[](vec3(0.5),vec3(0.5,0.5,-0.5),vec3(0.5,-0.5,0.5),vec3(0.5,-0.5,-0.5),vec3(-0.5,0.5,0.5),vec3(-0.5,0.5,-0.5),vec3(-0.5,-0.5,0.5),vec3(-0.5));layout(binding=1,std430)restrict readonly buffer BlobOctree{int blob_ot[];}_151;layout(binding=0,std430)restrict readonly buffer Blobs{vec4 blobs[];}_268;layout(location=0)uniform int blob_count;layout(location=2)uniform float sdf_size;layout(location=3)uniform int sdf_res;layout(location=1)uniform vec3 sdf_pos;layout(location=5)uniform float blob_smooth;layout(location=4)uniform float sdf_max_dist;layout(binding=0,rgba8)uniform writeonly image3D img_output;vec3 colors[6];float dist_cube(vec3 c,float s,vec3 p){vec3 d=abs(p-c)-vec3(s*0.5);return length(max(d,vec3(0.0)));}float dist_sphere(vec3 c,float r,vec3 p){return length(p-c)-r;}float smin(float a,float b,float k){float h=clamp(0.5+((0.5*(b-a))/k),0.0,1.0);return mix(b,a,h)-((k*h)*(1.0-h));}void main(){colors=vec3[](vec3(0.4099999964237213134765625,0.039999999105930328369140625,0.0599999986588954925537109375),vec3(0.4900000095367431640625,0.800000011920928955078125,0.20000000298023223876953125),vec3(0.85000002384185791015625,0.767000019550323486328125,0.1360000073909759521484375),vec3(0.939999997615814208984375,0.56099998950958251953125,0.065800003707408905029296875),vec3(0.1500000059604644775390625,0.085000000894069671630859375,0.0),vec3(0.50499999523162841796875,0.70599997043609619140625,0.87000000476837158203125));bool use_octree=blob_count==(-1);vec3 p=(((vec3(gl_GlobalInvocationID)+vec3(0.5))*(sdf_size/float(sdf_res)))+sdf_pos)-(vec3(sdf_size)*0.5);int node_idx;int node_leaf_blob_count;if(use_octree){vec3 node_pos=sdf_pos;float node_size=sdf_size;node_idx=0;node_leaf_blob_count=_151.blob_ot[node_idx];while(node_leaf_blob_count==(-1)){bool found_intersection=false;for(int i=0;i<8;i++){vec3 child_pos=node_pos+((vec3(node_size)*_188[i])*0.5);float child_size=node_size*0.5;vec3 param=child_pos;float param_1=child_size;vec3 param_2=p;if(dist_cube(param,param_1,param_2)<=0.0){node_pos=child_pos;node_size=child_size;node_idx=_151.blob_ot[(node_idx+1)+i];node_leaf_blob_count=_151.blob_ot[node_idx];found_intersection=true;break;}}if(!found_intersection){break;}}}float value=1000.0;float color_total_influence=0.0;vec3 color=vec3(0.0);int count;if(use_octree){count=node_leaf_blob_count;}else{count=blob_count;}vec4 blob;for(int i_1=0;i_1<count;i_1++){if(use_octree){int blob_idx=_151.blob_ot[(node_idx+1)+i_1];blob=_268.blobs[blob_idx];}else{blob=_268.blobs[i_1];}int mat_idx=int(blob.w)%6;float radius=float(int(blob.w)/6)/10000.0;vec3 param_3=blob.xyz;float param_4=radius;vec3 param_5=p;float d=dist_sphere(param_3,param_4,param_5);float param_6=value;float param_7=d;float param_8=blob_smooth;value=smin(param_6,param_7,param_8);float color_influence=clamp(blob_smooth-d,0.0,1.0);color_influence*=color_influence;color_total_influence+=color_influence;color+=(colors[mat_idx]*color_influence);}value=clamp(value,-0.20000000298023223876953125,sdf_max_dist);color/=vec3(color_total_influence);imageStore(img_output,ivec3(gl_GlobalInvocationID),vec4(color,1.0-((value-(-0.20000000298023223876953125))/(sdf_max_dist-(-0.20000000298023223876953125)))));}\n";
const char *RAYMARCH_FRAG_SRC = "#version 430\nlayout(location=4)uniform float dist_scale;layout(binding=0)uniform sampler3D sdf_tex;layout(location=2)uniform mat4 proj_mat;layout(location=1)uniform mat4 view_mat;layout(location=0)uniform mat4 model_mat;layout(location=5)uniform float sdf_max_dist;layout(location=3)uniform vec3 cam_pos;layout(location=0)in vec3 local_pos;layout(location=0)out vec4 out_color;vec2 intersect_aabb(vec3 ro,vec3 rd,vec3 bmin,vec3 bmax){vec3 tmin=(bmin-ro)/rd;vec3 tmax=(bmax-ro)/rd;vec3 t1=min(tmin,tmax);vec3 t2=max(tmin,tmax);float tnear=max(max(t1.x,t1.y),t1.z);float tfar=min(min(t2.x,t2.y),t2.z);return vec2(tnear,tfar);}vec3 get_normal_at(vec3 p){vec3 small_step=vec3(0.20000000298023223876953125*dist_scale,0.0,0.0);vec3 uvw=p+vec3(0.5);float gradient_x=texture(sdf_tex,uvw+small_step.xyy).w-texture(sdf_tex,uvw-small_step.xyy).w;float gradient_y=texture(sdf_tex,uvw+small_step.yxy).w-texture(sdf_tex,uvw-small_step.yxy).w;float gradient_z=texture(sdf_tex,uvw+small_step.yyx).w-texture(sdf_tex,uvw-small_step.yyx).w;return-normalize(vec3(gradient_x,gradient_y,gradient_z));}vec4 ray_march(vec3 ro,vec3 rd){vec3 param=ro;vec3 param_1=rd;vec3 param_2=vec3(-0.5);vec3 param_3=vec3(0.5);vec2 near_far=intersect_aabb(param,param_1,param_2,param_3);float traveled=max(0.0,near_far.x);for(int i=0;i<128;i++){vec3 p=ro+(rd*traveled);vec4 dat=texture(sdf_tex,p+vec3(0.5));float dist=(((1.0-dat.w)*(sdf_max_dist-(-0.20000000298023223876953125)))+(-0.20000000298023223876953125))*dist_scale;if(dist<=0.001000000047497451305389404296875){if(dist<=(-0.00150000001303851604461669921875)){return vec4(dat.xyz*0.5,max(0.00999999977648258209228515625,traveled));}vec3 param_4=p;vec3 normal=get_normal_at(param_4);normal=normalize(transpose(inverse(mat3(model_mat[0].xyz,model_mat[1].xyz,model_mat[2].xyz)))*normal);float light0_val=mix(max(0.0,dot(normal,vec3(0.3244428336620330810546875,0.811107099056243896484375,0.4866642653942108154296875))),1.0,0.60000002384185791015625);float light1_val=mix(max(0.0,dot(normal,vec3(-0.1825741827487945556640625,-0.912870943546295166015625,-0.365148365497589111328125))),1.0,0.60000002384185791015625);vec3 color=((dat.xyz*vec3(1.0,1.0,0.89999997615814208984375))*light0_val)+((dat.xyz*vec3(0.0900000035762786865234375,0.0900000035762786865234375,0.20000000298023223876953125))*light1_val);return vec4(color,traveled);}traveled+=dist;bool _323=traveled>=40.0;bool _331;if(!_323){_331=traveled>=near_far.y;}else{_331=_323;}if(_331){break;}}return vec4(0.0,0.0,0.0,-1.0);}float get_depth_at(vec3 p){vec4 clip_pos=((proj_mat*view_mat)*model_mat)*vec4(p,1.0);float ndc_depth=clip_pos.z/clip_pos.w;return(((1.0*ndc_depth)+0.0)+1.0)/2.0;}void main(){vec3 cam_mdl_pos=(inverse(model_mat)*vec4(cam_pos,1.0)).xyz;vec3 ro=cam_mdl_pos;vec3 rd=normalize(local_pos-ro);vec3 param=ro;vec3 param_1=rd;vec4 result=ray_march(param,param_1);if(result.w<0.0){discard;}out_color=vec4(result.xyz,1.0);vec3 param_2=ro+(rd*result.w);gl_FragDepth=get_depth_at(param_2);}\n";
const char *RAYMARCH_VERT_SRC = "#version 430\nlayout(location=2)uniform mat4 proj_mat;layout(location=1)uniform mat4 view_mat;layout(location=0)uniform mat4 model_mat;layout(location=0)out vec3 local_pos;layout(location=0)in vec3 in_pos;void main(){local_pos=in_pos;gl_Position=((proj_mat*view_mat)*model_mat)*vec4(in_pos,1.0);}\n";
const char *SKYBOX_FRAG_SRC = "#version 430\nlayout(binding=0)uniform samplerCube skybox;layout(location=0)out vec4 out_color;layout(location=0)in vec3 local_pos;void main(){out_color=texture(skybox,local_pos);}\n";
const char *SKYBOX_VERT_SRC = "#version 430\nlayout(location=0)uniform mat4 view_mat;layout(location=1)uniform mat4 proj_mat;layout(location=0)out vec3 local_pos;layout(location=0)in vec3 in_pos;void main(){local_pos=in_pos;mat4 view_center=view_mat;view_center[3].x=0.0;view_center[3].y=0.0;view_center[3].z=0.0;gl_Position=(proj_mat*view_center)*vec4(in_pos,1.0);}\n";
