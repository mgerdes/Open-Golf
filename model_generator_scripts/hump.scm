(define (generate N height width length texture_width texture_height 
                  border_height border_width bp0x bp0y bp1x bp1y bp2x bp2y bp3x bp3y
                  ground_friction ground_cor ground_vel_scale
                  wall_friction wall_cor wall_vel_scale)
  (define-macro (while test . body) 
                `(call-with-exit
                   (lambda (break) 
                     (let continue ()
                       (if (let () ,test)
                           (begin 
                             (let () ,@body)
                             (continue))
                           (break))))))

  (define (bezier p0 p1 p2 p3 t)
    (define p 0)
    (define t1 (- 1 t))
    (set! p (+ p (* p0 t1 t1 t1)))
    (set! p (+ p (* p1 3 t t1 t1)))
    (set! p (+ p (* p2 3 t t t1)))
    (set! p (+ p (* p3 t t t)))
    p
    )

  (define (distance x0 y0 x1 y1)
    (define dx (- x1 x0))
    (define dy (- x1 x0))
    (sqrt (+ (* dx dx) (* dy dy))))

  (define points_idx 0)
  (define points (make-vector (round (* 8 (+ N 1)))))
  (define (add_point x y z)
    (terrain_model_add_point x y z)
    (vector-set! points points_idx (list x y z))
    (set! points_idx (+ points_idx 1))
    )

  (define total_dist 0)
  (define p0x 0)
  (define p0y 0)
  (define i 0)
  (while (< i (+ N 1))
         (define t (/ i N))
         (define y (* height (bezier bp0y bp1y bp2y bp3y t)))
         (define z (* length (bezier bp0x bp1x bp2x bp3x t)))
         (add_point (- border_width)           (- border_height)       z)
         (add_point (- border_width)           (+ y border_height)     z)
         (add_point 0                          (+ y border_height)     z)
         (add_point 0                          y                       z)
         (add_point width                      y                       z)
         (add_point width                      (+ y border_height)     z)
         (add_point (+ width border_width)     (+ y border_height)     z)
         (add_point (+ width border_width)     (- border_height)       z)
         (set! total_dist (+ total_dist (distance p0x p0y z y)))
         (set! p0x z)
         (set! p0y y)
         (set! i (+ i 1))
         )

  (define dist0 0)
  (set! i 0)
  (while (< i N)
         (define t0 (/ i N))
         (define t1 (/ (+ i 1) N))
         (define y0 (* height (bezier bp0y bp1y bp2y bp3y t0)))
         (define y1 (* height (bezier bp0y bp1y bp2y bp3y t1)))
         (define z0 (* length (bezier bp0x bp1x bp2x bp3x t0)))
         (define z1 (* length (bezier bp0x bp1x bp2x bp3x t1)))
         (define dist1 (+ dist0 (distance z0 y0 z1 y1)))

         (define num_points 4)
         (define mat_idx 1)
         (define smooth_normal 0)
         (define cor wall_cor)
         (define friction wall_friction)
         (define vel_scale wall_vel_scale)
         (define x (+ (* 8 i) 9))
         (define y (+ (* 8 i) 1))
         (define z (+ (* 8 i) 0))
         (define w (+ (* 8 i) 8))
         (define p0 (vector-ref points x))
         (define p1 (vector-ref points y))
         (define p2 (vector-ref points z))
         (define p3 (vector-ref points w))
         (define tc0x (* 2 (- (cadr p0) border_height)))
         (define tc0y z1)
         (define tc1x (* 2 (- (cadr p1) border_height))) 
         (define tc1y z0)
         (define tc2x (* 2 (- (cadr p2) border_height))) 
         (define tc2y z0)
         (define tc3x (* 2 (- (cadr p3) border_height)))
         (define tc3y z1)
         (define texture_coord_scale 0.25)
         ; Border-Side 1
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)

         (set! x (+ (* 8 i) 11))
         (set! y (+ (* 8 i) 3))
         (set! z (+ (* 8 i) 2))
         (set! w (+ (* 8 i) 10))
         (set! tc0x (* 2 border_height))
         (set! tc0y z1)
         (set! tc1x (* 2 border_height)) 
         (set! tc1y z0)
         (set! tc2x (* 2 border_width)) 
         (set! tc2y z0)
         (set! tc3x (* 2 border_width)) 
         (set! tc3y z1)
         ; Border-Side 2
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)

         (set! x (+ (* 8 i) 13))
         (set! y (+ (* 8 i) 5))
         (set! z (+ (* 8 i) 4))
         (set! w (+ (* 8 i) 12))
         (set! tc0x (* 2 border_width))
         (set! tc0y z1)
         (set! tc1x (* 2 border_width))
         (set! tc1y z0)
         (set! tc2x (* 2 border_height))
         (set! tc2y z0)
         (set! tc3x (* 2 border_height))
         (set! tc3y z1)
         ; Border-Side 3
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)

         (set! x (+ (* 8 i) 15))
         (set! y (+ (* 8 i) 7))
         (set! z (+ (* 8 i) 6))
         (set! w (+ (* 8 i) 14))
         (define p0 (vector-ref points x))
         (define p1 (vector-ref points y))
         (define p2 (vector-ref points z))
         (define p3 (vector-ref points w))
         (define tc0x (* 2 (- (cadr p0) border_height)))
         (define tc0y z1)
         (define tc1x (* 2 (- (cadr p1) border_height))) 
         (define tc1y z0)
         (define tc2x (* 2 (- (cadr p2) border_height))) 
         (define tc2y z0)
         (define tc3x (* 2 (- (cadr p3) border_height)))
         (define tc3y z1)
         ; Border-Side 4
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)


         (set! cor ground_cor)
         (set! friction ground_friction)
         (set! vel_scale ground_vel_scale)
         (set! x (+ (* 8 i) 10))
         (set! y (+ (* 8 i) 2))
         (set! z (+ (* 8 i) 1))
         (set! w (+ (* 8 i) 9))
         (set! tc0x (* 2 border_width))
         (set! tc0y z1) 
         (set! tc1x (* 2 border_width))
         (set! tc1y z0)
         (set! tc2x (* 2 0))
         (set! tc2y z0)
         (set! tc3x (* 2 0))
         (set! tc3y z1)
         ; Border-Top 1
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)

         (set! x (+ (* 8 i) 14))
         (set! y (+ (* 8 i) 6))
         (set! z (+ (* 8 i) 5))
         (set! w (+ (* 8 i) 13))
         (set! tc0x (* 2 0)) 
         (set! tc0y z1)
         (set! tc1x (* 2 0))
         (set! tc1y z0)
         (set! tc2x (* 2 border_width))
         (set! tc2y z0)
         (set! tc3x (* 2 border_width))
         (set! tc3y z1)
         ; Border-Top 2
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)

         (set! mat_idx 0)
         (set! x (+ (* 8 i) 12))
         (set! y (+ (* 8 i) 4))
         (set! z (+ (* 8 i) 3))
         (set! w (+ (* 8 i) 11))
         (set! tc0x 0)
         (set! tc0y (* texture_height (/ dist1 total_dist)))
         (set! tc1x 0)
         (set! tc1y (* texture_height (/ dist0 total_dist)))
         (set! tc2x texture_width)
         (set! tc2y (* texture_height (/ dist0 total_dist)))
         (set! tc3x texture_width)
         (set! tc3y (* texture_height (/ dist1 total_dist)))
         (set! texture_coord_scale 0.5)
         ; Ground-Top
         (terrain_model_add_face num_points mat_idx smooth_normal
                                 x y z w 
                                 tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                                 cor friction vel_scale)

         (set! dist0 dist1)
         (set! i (+ i 1))
         )

  ; Front and Back
  (begin
    (define num_points 4)
    (define mat_idx 1)
    (define smooth_normal 0)
    (define cor wall_cor)
    (define friction wall_friction)
    (define vel_scale wall_vel_scale)
    (define x 0)
    (define y 1)
    (define z 2)
    (define w 3)
    (define p0 (vector-ref points x))
    (define p1 (vector-ref points y))
    (define p2 (vector-ref points z))
    (define p3 (vector-ref points w))
    (define tc0x (cadr p0)) 
    (define tc0y (* 0.25 (car p0)))
    (define tc1x (cadr p1)) 
    (define tc1y (* 0.25 (car p1)))
    (define tc2x (cadr p2)) 
    (define tc2y (* 0.25 (car p2)))
    (define tc3x (cadr p3))
    (define tc3y (* 0.25 (car p3)))
    (define texture_coord_scale 1)
    (terrain_model_add_face num_points mat_idx smooth_normal
                            x y z w 
                            tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                            cor friction vel_scale)

    (set! x 4)
    (set! y 5)
    (set! z 6)
    (set! w 7)
    (define p0 (vector-ref points x))
    (define p1 (vector-ref points y))
    (define p2 (vector-ref points z))
    (define p3 (vector-ref points w))
    (define tc0x (cadr p0)) 
    (define tc0y (* 0.25 (car p0)))
    (define tc1x (cadr p1)) 
    (define tc1y (* 0.25 (car p1)))
    (define tc2x (cadr p2)) 
    (define tc2y (* 0.25 (car p2)))
    (define tc3x (cadr p3))
    (define tc3y (* 0.25 (car p3)))
    (terrain_model_add_face num_points mat_idx smooth_normal
                            x y z w 
                            tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                            cor friction vel_scale)

    (set! x (round (+ (* 8 N) 3)))
    (set! y (round (+ (* 8 N) 2)))
    (set! z (round (+ (* 8 N) 1)))
    (set! w (round (+ (* 8 N) 0)))
    (define p0 (vector-ref points x))
    (define p1 (vector-ref points y))
    (define p2 (vector-ref points z))
    (define p3 (vector-ref points w))
    (define tc0x (* 0.5 (- (cadr p0) border_height)))
    (define tc0y (* 0.25 (car p0)))
    (define tc1x (* 0.5 (- (cadr p1) border_height)))
    (define tc1y (* 0.25 (car p1)))
    (define tc2x (* 0.5 (- (cadr p2) border_height)))
    (define tc2y (* 0.25 (car p2)))
    (define tc3x (* 0.5 (- (cadr p3) border_height)))
    (define tc3y (* 0.25 (car p3)))
    (terrain_model_add_face num_points mat_idx smooth_normal
                            x y z w 
                            tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                            cor friction vel_scale)

    (set! x (round (+ (* 8 N) 0)))
    (set! y (round (+ (* 8 N) 7)))
    (set! z (round (+ (* 8 N) 4)))
    (set! w (round (+ (* 8 N) 3)))
    (define p0 (vector-ref points x))
    (define p1 (vector-ref points y))
    (define p2 (vector-ref points z))
    (define p3 (vector-ref points w))
    (define tc0x (* 0.5 (- (cadr p0) border_height)))
    (define tc0y (* 0.25 (car p0)))
    (define tc1x (* 0.5 (- (cadr p1) border_height)))
    (define tc1y (* 0.25 (car p1)))
    (define tc2x (* 0.5 (- (cadr p2) border_height)))
    (define tc2y (* 0.25 (car p2)))
    (define tc3x (* 0.5 (- (cadr p3) border_height)))
    (define tc3y (* 0.25 (car p3)))
    (terrain_model_add_face num_points mat_idx smooth_normal
                            x y z w 
                            tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                            cor friction vel_scale)

    (set! x (round (+ (* 8 N) 7)))
    (set! y (round (+ (* 8 N) 6)))
    (set! z (round (+ (* 8 N) 5)))
    (set! w (round (+ (* 8 N) 4)))
    (define p0 (vector-ref points x))
    (define p1 (vector-ref points y))
    (define p2 (vector-ref points z))
    (define p3 (vector-ref points w))
    (define tc0x (* 0.5 (- (cadr p0) border_height)))
    (define tc0y (* 0.25 (car p0)))
    (define tc1x (* 0.5 (- (cadr p1) border_height)))
    (define tc1y (* 0.25 (car p1)))
    (define tc2x (* 0.5 (- (cadr p2) border_height)))
    (define tc2y (* 0.25 (car p2)))
    (define tc3x (* 0.5 (- (cadr p3) border_height)))
    (define tc3y (* 0.25 (car p3)))
    (terrain_model_add_face num_points mat_idx smooth_normal
                            x y z w 
                            tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y texture_coord_scale
                            cor friction vel_scale)
    )
  )
