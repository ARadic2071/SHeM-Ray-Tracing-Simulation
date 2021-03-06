% import_plate.m
%
% Copyright (c) 2018-19, Sam Lambrick.
% All rights reserved.
% This file is part of the SHeM Ray Tracing Simulation, subject to the 
% GNU/GPL-3.0-or-later.
%
% Model funciton for importing a new type of pinhole plate. Provide the name of
% the stl CAD file and then adapat the rotation/movement of the pinholep plate
% to put it into the right place.
%
% Calling syntax:
%  pinhole_surface = import_plate(accuracy);
%
% INPUT:
%  plate_fname - name of the stl file including the full path
%
% OUPUT:
%  pinhole_surface - TriagSurface object containing the triangulation of the
%                     pinhole plate.
function pinhole_surface = import_newPlate(accuracy)
    if nargin == 0
        accuracy = 'low';
    end
    
    switch accuracy
        case 'low'
            plate_fname = 'pinholePlates/newChamberPlate_low.stl';
        case 'medium'
            plate_fname = 'pinholePlates/newChamberPlate_medium.stl';
        case 'high'
            plate_fname = 'pinholePlates/newChamberPlate_high.stl';
        otherwise
            error('Enter a correct pinhole plate accuracy');
    end
    
    % Import data from file.
    [F, V, N] = stlread(plate_fname);
    
    % Completely diffuse scattering as the 
    C = 1 + zeros(size(F,1), 1);
    P = zeros(size(F,1), 1);

    % Put inot a TiagSurface object
    pinhole_surface = TriagSurface(V, N, F, C, P);
    
    % Align the plate to match the simulation
    pinhole_surface.plate_align;
end

