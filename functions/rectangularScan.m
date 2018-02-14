% Copyright (c) 2018, Sam Lambrick.
% All rights reserved.
% This file is part of the SHeM Ray Tracing Simulation, subject to the 
% GNU/GPL-3.0-or-later.

function square_scan_info = rectangularScan(sample_surface, xrange, zrange, ...
        ray_pos, ray_dir, raster_movement, maxScatter, pinhole_surface, ...
        effuse_dir, effuse_pos, make_sphere, dist_to_sample, sphere_r, diffuse, ...
        thePath)
% rectangularScan.m
%
% Generates a 2d simulated image. Utilises a parfor loop to parellise the code.
%
% INPUTS:
%  sample_surface  - A TriagSurface object containing the triangulated mesh
%                    surface of the sample
%  xrange          - A two element vector containing the range of positions
%                    between which the scan is to be taken in the x 
%                    direction [low, high] in mm
%  zrange          - A two element vector containing the range of positions
%                    between which the scan is to be taken in the z 
%                    direction [low, high] in mm
%  ray_pos         - A 3xn array of initial ray positions usually generated by
%                    the function create_strating_rays.m 
%  ray_dir         - A 3xn array of initial ray directions usually generated by
%                    the function create_strating_rays.m
%  raster_movment  - The raster movment (in mm) betwen pixels
%  maxScatter      - The maximum number of scatters a single ray is allowed
%                    to undergo
%  pinhole_surface - A TriagSurface object containing the triangulated mesh
%                    surface of the pinhole plate.
%  effuse_dir      - A 3xn array of initial ray positions for the effuse beam.
%  effuse_pos      - A 3xn array of initial ray directions for the effuse beam.
%  make_sphere     - 1 or 0, to include an analytic sphere in the simulaiton or
%                    not (1 = yes).
%  dist_to_sample  - Uses for the case when an analytic sphere is included. The
%                    distance between the flat sample the sphere sits on and the
%                    pinhole plate.
%  sphere_r        - Used for the case when an analytic sphere is included. The
%                    radius of the sphere.
%  diffuse         - Defines the scattering off of the analytic sphere.
%                     0 = specular
%                     1 = cosine
%                     2 = uniform
%                     0<d<1 = a combination of cosine and specular. The value is
%                             the proportion that is cosine.
%  thePath         - The directory to which the results for this simulation
%                    are to be saved.
%
% OUTPUTS:
%  square_scan_info - An object of class RectangleInfo that contains all
%                     the results and information about the simulation
    
    % Dependent variables
    n_rays = length(ray_pos);
    n_effuse = length(effuse_pos);
    
    % The sample positions
    sample_xs = xrange(1):raster_movement:xrange(2);
    sample_zs = zrange(1):raster_movement:zrange(2);
    nx_pixels = length(sample_xs);
    nz_pixels = length(sample_zs);
    
    % Move the sample to the corner of the positions
    sample_surface.moveBy([xrange(1) 0 zrange(1)]);
    
    % Create the variables for output data
    counters = zeros(maxScatter, nz_pixels, nx_pixels);
    effuse_counters = zeros(nz_pixels, nx_pixels);
    num_killed = zeros(nz_pixels, nx_pixels);
    
    % Produce a time estimage for the simulation and print it out. This is
    % nessacerily a rough estimate.
    N_pixels = nx_pixels*nz_pixels;
    
    % Estimate of the time for the simulation
    t_estimate = time_estimate(n_rays, n_effuse, sample_surface, N_pixels);
    
    tic
    
    % Starts the parallel pool if one does not already exist.
    if isempty(gcp('nocreate'))
       parpool 
    end
    
    % Generates a graphical progress bar if we are using the MATLAB GUI.
    progressBar = feature('ShowFigureWindows');
    if progressBar
        ppm = ParforProgMon('Simulation progress: ', N_pixels);
    else
        % If the variable ppm is undefined then the parfor loop will
        % throw errors.
        ppm = 0;
    end
    
    parfor i_=1:N_pixels
        % The x and z pixels we are on
        z_pix = mod(i_, nz_pixels) - 1;
        if mod(i_, nz_pixels) == 0
            x_pix = floor(i_/nz_pixels) - 1;
        else
            x_pix = floor(i_/nz_pixels);
        end
        
        if z_pix == -1
            z_pix = nz_pixels - 1;
        end
        
        % Place the sample into the right position for this pixel
        this_surface = copy(sample_surface);
        this_surface.moveBy([raster_movement*x_pix, 0, raster_movement*z_pix]);
        
        if make_sphere
            scan_pos_x = xrange(1) + raster_movement*x_pix; %#ok<*PFBNS>
            scan_pos_z = zrange(1) + raster_movement*z_pix;
        else
            scan_pos_x = 0;
            scan_pos_z = 0;
        end
        
        % The main beam
        [~, killed, ~, ~, ~, numScattersRayDetect, ~] = ...
            trace_rays(ray_pos, ray_dir, this_surface, maxScatter, ...
                       pinhole_surface, scan_pos_x, scan_pos_z, make_sphere, ...
                       dist_to_sample, sphere_r, diffuse);
        
        % The effusive beam
        if size(effuse_pos, 1) == 0
            % If the effusive beam is not being modelled
            effuse_cntr = 0;
        else
            [effuse_cntr, ~, ~, ~, ~, ~, ~] = ...
                trace_rays(effuse_pos, effuse_dir, this_surface, maxScatter, ...
                           pinhole_surface, scan_pos_x, scan_pos_z, ...
                           make_sphere, dist_to_sample, sphere_r, diffuse);
        end
        
        % Bin the results by the number of sample scattering events they
        % underwent.
        histRay = binMyWay(numScattersRayDetect, maxScatter);
        
        % Update the progress bar if we are working in the MATLAB GUI.
        if progressBar
            ppm.increment();
        end
        
        counters(:,i_) = histRay;
        num_killed(i_) = killed;
        effuse_counters(i_) = effuse_cntr;
        
        delete(this_surface);
    end
    
    % Close the parallel pool
    current_pool = gcp('nocreate');
    delete(current_pool);
    
    t = toc;
    
    % Actual time taken
    fprintf('Actual time taken: %f s\n', t);
    hr = floor(t/(60^2));
    min = floor((t - hr*60*60)/60);
    if min == 60
        hr = hr + 1;
        min = 0;
    end
    fprintf('Which is: %i hr %2i mins\n\n', hr, min);
    
    % Generate output square scan class
    square_scan_info = RectangleInfo(counters, num_killed, sample_surface, ...
        xrange, zrange, raster_movement, n_rays, t, t_estimate, effuse_counters);
    
    % Draw and save images
    % All the images are saved giving maximum contrast in the images:
    %  black - pixel with fewest counts
    %  white - pixel with the most counts
    % Only draws them if there is a GUI.
    if progressBar
        square_scan_info.produceImages(thePath);
    end
end