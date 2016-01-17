#version 110
void main() {
	///*
	vec4 pos = ftransform();
	float z = (pos.z / pos.w + 1.0) / 2.0;
	gl_FrontColor = vec4(z, z, z, 1.0);
	gl_Position = pos;
	//*/
	//gl_Position = ftransform();
}