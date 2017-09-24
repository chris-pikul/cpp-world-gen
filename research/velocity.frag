
Shader "Custom/VelocityShader" 
{
	Properties 
	{
    	_MainTex("HtTex", 2D) = "black" { }
    	_FluxTex("FluxTex", 2D) = "black" { }
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
			sampler2D _FluxTex;
			float _TexSize;
			
			float FindSlope(float2 uv, float u)
			{
			
				float2 offsets[4];
				offsets[0] = uv + float2(-u, 0);
				offsets[1] = uv + float2(u, 0);
				offsets[2] = uv + float2(0, -u);
				offsets[3] = uv + float2(0, u);
				
				float hts[4];
				for(int i = 0; i < 4; i++)
				{
					hts[i] = tex2D(_MainTex, offsets[i]).x;
				}
				
				float2 _step = float2(1.0, 0.0);
				
				float3 va = normalize( float3(_step.xy, hts[1]-hts[0]) );
			    float3 vb = normalize( float3(_step.yx, hts[3]-hts[2]) );
			    
			    float3 n = cross(va,vb).rbg;
			    
			    //you dont want slope to be zero
			   	return max(0.1, 1.0-abs(dot(n, float3(0,1,0))));
			}
			
			float4 frag(v2f IN) : COLOR
			{
				//If you increase time you must increase pipe length
				//The function is not designed to be continuous over large time scale so I would leave it as is
				const float T = 0.1f; 	//delta time
				const float L = 1.0f; 	//pipe length
				const float Kc = 0.04f; //sediment capacity constant
				const float Ks = 0.04f; //dissolving constant
				const float Kd = 0.04f; //deposition constant
				const float Ke = 0.01;	//evaporation constant
				const float Kr = 0.001; //rain constant
				float u = 1.0f/_TexSize;
				
				float3 texelHts = tex2D(_MainTex, IN.uv).xyz;
				
				float4 texelFlux = tex2D(_FluxTex, IN.uv);
				float4 texelFluxL = tex2D(_FluxTex, IN.uv + float2(-u, 0));
				float4 texelFluxR = tex2D(_FluxTex, IN.uv + float2(u, 0));
				float4 texelFluxT = tex2D(_FluxTex, IN.uv + float2(0, u));
				float4 texelFluxB = tex2D(_FluxTex, IN.uv + float2(0, -u));
				
				//Flux in is inlow from neighour cells. Note for the cell on the left you need thats cells flow to the right (ie it flows into this cell)
				float fluxIN = texelFluxL.y + texelFluxR.x + texelFluxT.w + texelFluxB.z;
				
				//Flux out is all out flows from this cell
				//left(x), right(y), top(z), bottom(w)
				float fluxOUT = texelFlux.x + texelFlux.y + texelFlux.z + texelFlux.w;
				
				//V is net volume change for the water over time
				float V = T * (fluxIN - fluxOUT);
				
				//waterHt is equally to previously calculated ht plus rain amount
				float rain = 0.1f;
				float waterHt = texelHts.z + rain * T * Kr;

				//The water ht is the previously calculated ht plus the net volume change divided by lenght squared
				waterHt = waterHt + V / (L*L);
				
				//remove some water due evaportion scaled by evaporation constant (Ke)
				waterHt *= 1.0f - (Ke * T);
				
				//The average amount of water that passes through cell per unit time from left to right (x) and top to bottom (y) is the velocity field
				float2 velocityField;
				velocityField.x = texelFluxL.y - texelFlux.x + texelFlux.y - texelFluxR.x;
				velocityField.y = texelFluxT.w - texelFlux.z + texelFlux.w - texelFluxB.z;
				velocityField *= 0.5f;
				
				//In the original algorithm the slope is used so flat areas are eroded slower than sloped areas
				//I found that leaving it out create better river like structures.
				//float slope = FindSlope(IN.uv, u);
				
				//The sediment transport capacity (C) is the length of velocity * sediment capacity constant (Kc)
				float C = Kc * length(velocityField); // * slope;

				//The sediment value is the sediment value from last pass transported by velocity field
				float texelSediment = tex2D(_MainTex, IN.uv - velocityField/_TexSize).w;

				//If sediment transport capacity greater than sediment in water remove some land and add to sediment scaled by dissolving constant (Ks)
				//Else if sediment transport capacity less than sediment in water remove some sediment and add to land scaled by deposition constant (Kd)

				float sediment = texelSediment;
				float landHt = texelHts.x;
				float sandHt = texelHts.y;
				
				float KS = max(0, Ks * (C - texelSediment));
				float KD = max(0, Kd * (texelSediment - C));
				
				//If land (rock) is removed from erosion it then becomes sediment which is deposited as sand
				//The total land ht is land + sand ht so the sand has no extra effect in this version of the algorithm, it would be
				//the same as adding and remove from land alone. You my how ever wish to treat the sand differently to make a more realistic version
				
				if(C > texelSediment && landHt - KS > 0.0f && waterHt > texelSediment)
				{
					landHt -= KS;
					sediment += KS;
				}
				else
				{
					sandHt += KD;
					sediment -= KD;
				}

				return float4(landHt, sandHt, waterHt, max(0.0, sediment));
			}
			
		ENDCG

    	}
	}
}