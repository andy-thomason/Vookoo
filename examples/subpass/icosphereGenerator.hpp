void generateIcosphereUnordered(std::vector<float>* vertices, std::vector<unsigned int>* indices, int samples)
{
    // [https://github.com/egeozgul/3D-Icosphere-Generator/blob/master/icosphereGenerator.hpp]
    auto insertVertex = [](std::vector<float>* vertices_, float a_, float b_, float c_) {
      vertices_->push_back(a_);
      vertices_->push_back(b_);
      vertices_->push_back(c_);
    };
    auto addTriangle = [](std::vector<unsigned int>* indices_, int a_, int b_, int c_) {
      indices_->push_back(a_);
      indices_->push_back(b_);
      indices_->push_back(c_);
    };

    float triangleSideLength = 1 / pow(2.0, (double)samples); 

    unsigned int meshStartingIndex = vertices->size();

    int nSideTriangles = pow(2.0, (double)samples);

    //samples > 0
    for (int iy = -nSideTriangles; iy <= 0; iy++)
    {
        int from  = -nSideTriangles + abs(iy);
        int to = abs(from);
        
        for (int ix = from; ix <= to; ix++)
        {
            float x = (float)ix* triangleSideLength;
            float y = (float)iy* triangleSideLength;

            float z = x + y + 1;
            if(ix>0)
                z = -x + y + 1;
            insertVertex(vertices, x, z,y);
        }
        
        for (int ix = from+1; ix <= to-1; ix++)
        {
            float x = (float)ix * triangleSideLength;
            float y = (float)iy * triangleSideLength;

            float z = -x - y - 1;
            if (ix > 0)
                z = x - y - 1;

            insertVertex(vertices, x, z, y);
        }
    }

    for (int iy = 1; iy <= nSideTriangles; iy++)
    {
        int from = -nSideTriangles + abs(iy);
        int to = abs(from);

        for (int ix = from; ix <= to; ix++)
        {
            float x = (float)ix * triangleSideLength;
            float y = (float)iy * triangleSideLength;
            
            float z = x - y + 1;
            if (ix > 0)
                z = -x - y + 1;
            
            insertVertex(vertices, x, z, y);
        }

        for (int ix = from + 1; ix <= to - 1; ix++)
        {
            float x = (float)ix * triangleSideLength;
            float y = (float)iy * triangleSideLength;
            float z = -x + y - 1;
            if (ix > 0)
                z = x + y - 1;
            insertVertex(vertices, x, z, y);
        }
    }

    
    std::vector<int> list;
    
    list.resize(nSideTriangles*2+1);

    
    int previousIndex = 1;
    int currentIndex = 0;
    for (int iy = 1; iy < nSideTriangles +1+1; iy++)
    {   
        currentIndex = previousIndex + 4 * (iy-1);
        list[iy] = currentIndex;
        previousIndex = currentIndex;
    }

    for (int iy = 1; iy < nSideTriangles; iy++)
    {
        float diff = list[(nSideTriangles+2) - iy-1] - list[(nSideTriangles+1) - iy-1];
        list[(nSideTriangles + 1) + iy] = diff + list[(nSideTriangles + 1) + iy - 1];

    }
    


    //topleft
    for (int iy = 1; iy < nSideTriangles + 1; iy++)
    {
        int prevStartingIndex = list[iy - 1];
        int currStartingIndex = list[iy];

        //topleft
        for (int ix = 0; ix < iy; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix;
            int bottomCurrentIndex = currStartingIndex + ix;
            addTriangle(indices, topCurrentIndex, bottomCurrentIndex, bottomCurrentIndex + 1);


            if (ix == 0)
            {
                addTriangle(indices, topCurrentIndex, bottomCurrentIndex, bottomCurrentIndex + (iy)*2 + 1);
            }
            else
            {
                addTriangle(indices, topCurrentIndex+ (iy-1) * 2, bottomCurrentIndex + iy * 2, bottomCurrentIndex + iy * 2 + 1);
            }
        }

        for (int ix = 0; ix < iy - 1; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix;
            int bottomCurrentIndex = currStartingIndex + ix;
            addTriangle(indices, topCurrentIndex, topCurrentIndex + 1, bottomCurrentIndex + 1);

            if (ix == 0)
            {
                addTriangle(indices, topCurrentIndex, topCurrentIndex + (iy-1) * 2 + 1, bottomCurrentIndex + (iy) * 2 + 1);
            }
            else
            {
                addTriangle(indices, topCurrentIndex + (iy - 1) * 2, topCurrentIndex + (iy - 1) * 2+1, bottomCurrentIndex + iy * 2 + 1);
            }
        }

        //topright
        for (int ix = 0; ix < iy; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix + iy - 1;
            int bottomCurrentIndex = currStartingIndex + ix + iy;
            addTriangle(indices, topCurrentIndex, bottomCurrentIndex, bottomCurrentIndex + 1);

            if (ix == iy-1)
            {
                addTriangle(indices, topCurrentIndex, bottomCurrentIndex + (iy) * 2 , bottomCurrentIndex + 1);
            }
            else
            {
                addTriangle(indices, topCurrentIndex + (iy - 1) * 2, bottomCurrentIndex + iy * 2, bottomCurrentIndex + iy * 2 + 1);
            }
        }

        for (int ix = 0; ix < iy - 1; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix + iy - 1;
            int bottomCurrentIndex = currStartingIndex + ix + iy;
            addTriangle(indices, topCurrentIndex, topCurrentIndex + 1, bottomCurrentIndex + 1);

            if (ix == iy - 1-1)
            {
                addTriangle(indices, topCurrentIndex + (iy-1) * 2, topCurrentIndex + 1, bottomCurrentIndex + (iy) * 2 + 1);
            }
            else
            {
                addTriangle(indices, topCurrentIndex + (iy - 1) * 2, topCurrentIndex + (iy - 1) * 2 + 1, bottomCurrentIndex + iy * 2 + 1);
            }
        }

    }
    
    
    //bottom
    int nT = nSideTriangles;
    for (int iy = nSideTriangles; iy <  2*nSideTriangles; iy++,nT--)
    {
        int prevStartingIndex = list[iy];
        int currStartingIndex = list[iy+1];

        //bottomright
        for (int ix = 0; ix < nT; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix ;
            int bottomCurrentIndex = currStartingIndex + ix;
            addTriangle(indices, topCurrentIndex, topCurrentIndex+1, bottomCurrentIndex);

            if (ix == 0)
            {
                addTriangle(indices, topCurrentIndex, topCurrentIndex + (nT) * 2 + 1, bottomCurrentIndex);
            }
            else
            {
                addTriangle(indices, topCurrentIndex + (nT) * 2, topCurrentIndex + (nT) * 2 + 1, bottomCurrentIndex + (nT-1) * 2);
            }
        }

        for (int ix = 0; ix < nT - 1; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix;
            int bottomCurrentIndex = currStartingIndex + ix;
            addTriangle(indices, topCurrentIndex + 1, bottomCurrentIndex, bottomCurrentIndex + 1);

            if (ix == 0)
            {
                addTriangle(indices, topCurrentIndex + (nT) * 2+1, bottomCurrentIndex, bottomCurrentIndex + (nT-1) * 2+1);
            }
            else
            {
                addTriangle(indices, topCurrentIndex + (nT) * 2+1, bottomCurrentIndex + (nT-1) * 2 , bottomCurrentIndex + (nT - 1) * 2+1);
            }
        }


        //topright
        for (int ix = 0; ix < nT; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix + nT;
            int bottomCurrentIndex = currStartingIndex + ix + nT-1;
            addTriangle(indices, topCurrentIndex, topCurrentIndex+1, bottomCurrentIndex);


            if (ix == nT-1)
            {
                addTriangle(indices, topCurrentIndex  +(nT) * 2, topCurrentIndex+1, bottomCurrentIndex); // problem???
            }
            else
            {
               addTriangle(indices, topCurrentIndex + (nT) * 2, topCurrentIndex  + nT * 2 + 1, bottomCurrentIndex + (nT-1) * 2);
            }
        }

        for (int ix = 0; ix < nT - 1; ix++)
        {
            int topCurrentIndex = prevStartingIndex + ix + nT;
            int bottomCurrentIndex = currStartingIndex + ix + nT-1;
            addTriangle(indices, topCurrentIndex+1, bottomCurrentIndex, bottomCurrentIndex + 1);

            if (ix == nT-2)
            {
                addTriangle(indices, topCurrentIndex + (nT) * 2 + 1, bottomCurrentIndex + (nT - 1) * 2, bottomCurrentIndex+1); // problem???
            }
            else
            {
                addTriangle(indices, topCurrentIndex + (nT) * 2+1, bottomCurrentIndex + (nT-1) * 2, bottomCurrentIndex + (nT - 1) * 2 + 1);
            }
        }

    }
}

void generateIcosphere(std::vector<float>* vertices, std::vector<unsigned int>* indices, int samples, bool CCW = true)
{
    generateIcosphereUnordered(vertices, indices, samples);

    // correct the winding order of all the triangles to be counter clockwise (CCW) or clockwise (CW)
    for(size_t i=0; i<(*indices).size()/3; i++) {
        unsigned int j = (*indices)[i*3+0],
                     k = (*indices)[i*3+1],
                     l = (*indices)[i*3+2];
        float *v0 = &(*vertices)[j*3]; // a triangle's vertices  
        float *v1 = &(*vertices)[k*3];  
        float *v2 = &(*vertices)[l*3];  
        float m0 = sqrt(v0[0]*v0[0] + v0[1]*v0[1] + v0[2]*v0[2]); // normalization scales
        float m1 = sqrt(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);
        float m2 = sqrt(v2[0]*v2[0] + v2[1]*v2[1] + v2[2]*v2[2]);
        float a[] = {(v0[0]/m0+v1[0]/m1+v2[0]/m2)/3.f, 
                     (v0[1]/m0+v1[1]/m1+v2[1]/m2)/3.f, 
                     (v0[2]/m0+v1[2]/m1+v2[2]/m2)/3.f}; // middle of triangle
        float b[] = {v2[0]/m2-v1[0]/m1, 
                     v2[1]/m2-v1[1]/m1, 
                     v2[2]/m2-v1[2]/m1}; // "lower" triangle edge
        float c[] = {v0[0]/m0-v1[0]/m1, 
                     v0[1]/m0-v1[1]/m1, 
                     v0[2]/m0-v1[2]/m1}; // "upper" triangle edge
        float dotcross =   a[0]*( b[1]*c[2] - b[2]*c[1])
                         + a[1]*( b[2]*c[0] - b[0]*c[2])
                         + a[2]*( b[0]*c[1] - b[1]*c[0]); // a <dot> (b <cross> c)
        // desire CCW (openGL), so wrong winding order if +dot(a,cross(b,c)) < 0
        // desire  CW (vulkan), so wrong winding order if -dot(a,cross(b,c)) < 0
        if ( (CCW?+1.:-1.) * dotcross < 0. ) {
            // swap winding order of the triangle indices
            (*indices)[i*3+0] = l;
            (*indices)[i*3+1] = k;
            (*indices)[i*3+2] = j;
        }
    }
}
