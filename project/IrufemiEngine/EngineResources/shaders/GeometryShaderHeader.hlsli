struct GeometryShaderOutput
{
	float32_t4 svpos : SV_POSITION;
	float32_t3 normal : NORMAL0;
	float32_t2 uv : TEXCOORD0;
	float32_t3 worldPosition : POSITION0;
};
