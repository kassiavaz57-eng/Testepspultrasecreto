        int h=(int)fminf((float)tileH,dstY+dstH-y);
        if(h<=0)break;
        for(float x=dstX;x<dstX+dstW-0.001f;x+=(float)tileW){
            int w=(int)fminf((float)tileW,dstX+dstW-x);
            if(w<=0)break;
            pspDrawSpritePart(renderer,tpagIndex,srcX,srcY,w,h,x,y,1.0f,1.0f,0.0f,x,y,color,alpha);
        }
    }
    (void)t;
}
static void pspDrawRectangle(Renderer *renderer,float x1,float y1,float x2,float y2,uint32_t color,float alpha,bool outline){
    (void)renderer; uint32_t c=bgrToGu(color,alpha); PSPVertex *v=(PSPVertex*)sceGuGetMemory(4*sizeof(PSPVertex));
    float tx1=(x1-g_viewX)*g_scaleX+g_offX, ty1=(y1-g_viewY)*g_scaleY+g_offY;
    float tx2=(x2-g_viewX)*g_scaleX+g_offX, ty2=(y2-g_viewY)*g_scaleY+g_offY;
    v[0]=(PSPVertex){0,0,c,tx1,ty1,0};v[1]=(PSPVertex){0,0,c,tx2,ty1,0};v[2]=(PSPVertex){0,0,c,tx2,ty2,0};v[3]=(PSPVertex){0,0,c,tx1,ty2,0};
    sceGuDisable(GU_TEXTURE_2D); sceGuDrawArray(outline?GU_LINE_STRIP:GU_TRIANGLE_FAN,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,NULL,v); sceGuEnable(GU_TEXTURE_2D);
}
