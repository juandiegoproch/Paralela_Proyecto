#include <cmath>
#include <vector>
#include <cstring>
#include <iostream>

int main(int argc, char** argv){
    
    // Parametros del problema
    // Parametros de resolucion de la grilla
    const unsigned int grid_x = 8;
    const unsigned int grid_y = 8;

    const double dominio_x_max = 1.0;
    const double dominio_y_max = 1.0;

    const int itmax = 20000;

    // Parametros calculados 
    double dx = dominio_x_max/grid_x;
    double dy = dominio_y_max/grid_y;
    double dt = std::min(dx*dx,dy*dy)/4.0; // elegido por criterio de estabilidad

    double dx2 = dx*dx, dy2 = dy*dy;

    // +2 para contener condiciones de frontera
    std::vector<double> phi((grid_x+2)*(grid_y+2));
    std::vector<double> phi_working((grid_x+2)*(grid_y+2));

    size_t phi_size = (grid_x+2)*(grid_y+2); // in count of doubles
    /*
        Nota: Por las diferencias de indexacion en el codigo original, se debe reescribir la inicializacion
        Se replica la intencion del codigo original:
        Arriba 1s abajo 0s y a los bordes una rampa
    */
    // condiciones iniciales y de frontera
    
    for (int i=0;i<grid_x+2;++i){
        for (int j=0;j<grid_y+2;j++){
            if (i == 0 || i == grid_x + 1) phi[i*(grid_x+2) + j] = j*dy; // y*dy ramp (bounduary cnd)
            else if (j == grid_y+1) phi[i*(grid_x+2) + j] = 1.0; // ones at the top (bounduary cnd)
            else phi[i*(grid_x+2) + j] = 0;
        }
    }
    
    std::copy(phi.begin(), phi.end(), phi_working.begin());
    /*
    for (int i = 0; i<grid_x+2;i++)
    {
        for (int j=0; j<grid_y+2;j++){
            std::cout << phi[(i)*(grid_x+2) + j] << ", ";
        }
        std::cout << "\n";
    }
    std::cout.flush();
    */

    // main loop
    for (int it=0;it<itmax;++it){
        for (int i=0;i<grid_x;++i){
            for (int j=0; j<grid_y;++j){

                int x_idx = i+1;
                int y_idx = j+1;

                double upper_neighbour = phi[x_idx*(grid_x+2) + y_idx - 1];
                double lower_neighbour = phi[x_idx*(grid_x+2) + y_idx + 1];
                double left_neighbour = phi[(x_idx - 1)*(grid_x+2) + y_idx];
                double right_neighbour = phi[(x_idx+1)*(grid_x+2) + y_idx];
                double this_cell = phi[(x_idx)*(grid_x+2) + y_idx];

                double dphi = ((right_neighbour+left_neighbour-2.0*this_cell)*dx2
                             +(lower_neighbour+upper_neighbour-2.0*this_cell)*dy2)
                             * dt;
                phi_working[(x_idx)*(grid_x+2) + y_idx] = this_cell + dphi;
            }
        }
        std::swap(phi,phi_working);
    }

    for (int i = 0; i<grid_x+2;i++)
    {
        for (int j=0; j<grid_y+2;j++){
            std::cout << phi[(i)*(grid_x+2) + j] << ", ";
        }
        std::cout << "\n";
    }

    std::cout.flush();

}