
Shader "Custom/NoiseShader" 
{
	Properties 
	{
    	_MainTex("MainTex", 2D) = "black" { }
    	_Frq("Frq", Float) = 1.0
    	_Amp("Amp", Float) = 1.0
    	_TexSize("TexSize", Float) = 1.0
    	_Octaves("Octaves", Float) = 8.0
	}
	SubShader 
	{
    	Pass 
    	{

			CGPROGRAM
			#pragma target 3.0
			#pragma only_renderers d3d9
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
			float _Frq;
			float _Amp;
			float _TexSize;
			float _Octaves;
			
			float FADE(float t) { return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f ); }
	
			float LERP(float t, float a, float b) { return (a) + (t)*((b)-(a)); }
			
			int PERM(int i)
			{
				float x = i % 16;
				float y = i / 16;
				return tex2D(_MainTex, float2((float)x/16.0f, (float)y/16.0f)).a * 255.0f;
			}
			
			float GRAD2(int hash, float x, float y)
			{
				int h = hash % 16;
		    	float u = h<4 ? x : y;
		    	float v = h<4 ? y : x;
				int hn = h % 2;
				int hm = (h/2) % 2;
		    	return ((hn != 0) ? -u : u) + ((hm != 0) ? -2.0f*v : 2.0f*v);
			}
			
			float NOISE(float x, float y)
			{
				int ix0, iy0, ix1, iy1;
			    float fx0, fy0, fx1, fy1, s, t, nx0, nx1, n0, n1;
			    
			    ix0 = floor(x); 		// Integer part of x
			    iy0 = floor(y); 		// Integer part of y
			    fx0 = x - ix0;        	// Fractional part of x
			    fy0 = y - iy0;        	// Fractional part of y
			    fx1 = fx0 - 1.0f;
			    fy1 = fy0 - 1.0f;
			    ix1 = (ix0 + 1) % 256; 	// Wrap to 0..255
			    iy1 = (iy0 + 1) % 256;
			    ix0 = ix0 % 256;
			    iy0 = iy0 % 256;
			    
			   	t = FADE( fy0 );
	    		s = FADE( fx0 );
	    		
	    		nx0 = GRAD2(PERM(ix0 + PERM(iy0)), fx0, fy0);
			    nx1 = GRAD2(PERM(ix0 + PERM(iy1)), fx0, fy1);
				
			    n0 = LERP( t, nx0, nx1 );
			
			    nx0 = GRAD2(PERM(ix1 + PERM(iy0)), fx1, fy0);
			    nx1 = GRAD2(PERM(ix1 + PERM(iy1)), fx1, fy1);
				
			    n1 = LERP(t, nx0, nx1);
			
			    return 0.507f * LERP( s, n0, n1 );
			    
			}
			
			float4 frag(v2f IN) : COLOR
			{
				float2 pos = IN.uv * _TexSize;
				
				float sum = 0.0;
				float gain = 1.0;
				for(int i = 0; i < 12; i++)
				{
					sum += NOISE((pos.x*gain) / _Frq, (pos.y*gain) / _Frq) * (_Amp/gain);
					gain *= 2.0;
				}
				
				sum = (sum+1.0f)*0.5f;

				return float4(sum,0,0,0);
			}
			
		ENDCG

    	}
	}
}