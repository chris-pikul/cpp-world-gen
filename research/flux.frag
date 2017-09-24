
Shader "Custom/FluxShader" 
{
	Properties 
	{
    	_MainTex("MainTex", 2D) = "black" { }
    	_HtTex("HtTex", 2D) = "black" { }
    	_TexSize("TexSize", Float) = 1.0
	}
	SubShader 
	{
    	Pass 
    	{

			CGPROGRAM
			#pragma target 3.0
			#pragma vertex vert
			#pragma fragment frag
			
			struct a2v 
			{
    			float4  pos : SV_POSITION;
    			float2  uv : TEXCOORD0;
			};

			struct v2f 
			{
    			float4  pos : SV_POSITION;
    			float2  uv : TEXCOORD0;
			};

			v2f vert(a2v v)
			{
    			v2f OUT;
    			OUT.pos = mul(UNITY_MATRIX_MVP, v.pos);
    			OUT.uv = v.uv;
    			return OUT;
			}
			
			sampler2D _MainTex;
			sampler2D _HtTex;
			float _TexSize;
			
			float4 frag(v2f IN) : COLOR
			{
				//If you increase time you must increase pipe length and cell area
				//The function is not designed to be continuous over large time scale so I would leave it as is
				const float T = 0.1f; //delta time
				const float L = 1.0f; //pipe lenght
				const float A = 1.0f; //cell area
				const float G = 9.81f;
				float u = 1.0f/_TexSize;
				
				float4 texelHts = tex2D(_HtTex, IN.uv);
				float4 texelHtsL = tex2D(_HtTex, IN.uv + float2(-u, 0));
				float4 texelHtsR = tex2D(_HtTex, IN.uv + float2(u, 0));
				float4 texelHtsT = tex2D(_HtTex, IN.uv + float2(0, u));
				float4 texelHtsB = tex2D(_HtTex, IN.uv + float2(0, -u));
				
				float4 texelFlux = tex2D(_MainTex, IN.uv);
				
				//ht at this cell is land ht(x) + sand ht(y) + water ht(z)
				float ht = texelHts.x + texelHts.y + texelHts.z;
				
				//deltaHX if the height diff between this cell and neighbour X
				float deltaHL = ht - texelHtsL.x - texelHtsL.y - texelHtsL.z;
				float deltaHR = ht - texelHtsR.x - texelHtsR.y - texelHtsR.z;
				float deltaHT = ht - texelHtsT.x - texelHtsT.y - texelHtsT.z;
				float deltaHB = ht - texelHtsB.x - texelHtsB.y - texelHtsB.z;
				
				//new flux value is old value + delta time * area * ((gravity * delta ht) / length)
				//max 0, no neg values
				//left(x), right(y), top(z), bottom(w)
				
				float fluxL = max(0.0, texelFlux.x + T * A * ((G * deltaHL) / L));
				float fluxR = max(0.0, texelFlux.y + T * A * ((G * deltaHR) / L));
				float fluxT = max(0.0, texelFlux.z + T * A * ((G * deltaHT) / L));
				float fluxB = max(0.0, texelFlux.w + T * A * ((G * deltaHB) / L));
				
				//If the sum of the outflow flux exceeds the water amount of the
				//cell, flux value will be scaled down by a factor K to avoid negative
				//updated water height
				
				float K = min(1.0, (texelHts.z * L*L) / ((fluxL + fluxR + fluxT + fluxB) * T) );
		
				//Clamp to edge, migth be a better way to do this.
				if(IN.uv.x == 0.0f) fluxL = 0.0;
				if(IN.uv.y == 0.0f) fluxB = 0.0;
				if(IN.uv.x == 1.0f) fluxR = 0.0;
				if(IN.uv.y == 1.0f) fluxT = 0.0;
				
				return float4(fluxL, fluxR, fluxT, fluxB) * K;
			
			}
			
		ENDCG

    	}
	}
}