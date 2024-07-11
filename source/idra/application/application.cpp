/*
 * Copyright 2024 Gabriel Sassone. All rights reserved.
 * License: https://github.com/JorenJoestar/Idra/blob/main/LICENSE
 */

#include "application/application.hpp"

namespace idra {


void Application::run( const ApplicationConfiguration& configuration ) {

    create( configuration );
    main_loop();
    destroy();
}

} // namespace idra