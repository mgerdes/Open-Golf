(define (generate N height width length texture_width texture_height 
                  texture_dx texture_dy
                  border_height border_width border_ground_y
                  bp0x bp0y bp1x bp1y bp2x bp2y bp3x bp3y
                  make_border make_back
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
         (add_point 0                          y                       z)
         (add_point width                      y                       z)
         (if (> make_border 0)
             (begin
               (add_point (- border_width)           border_ground_y         z)
               (add_point (- border_width)           (+ y border_height)     z)
               (add_point 0                          (+ y border_height)     z)
               (add_point width                      (+ y border_height)     z)
               (add_point (+ width border_width)     (+ y border_height)     z)
               (add_point (+ width border_width)     border_ground_y         z)
               )
             '()
             )
         (set! total_dist (+ total_dist (distance p0x p0y z y)))
         (set! p0x z)
         (set! p0y y)
         (set! i (+ i 1))
         )

 (define points_per_row 
   (if (> make_border 0) 8 2))

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
         (define mat_idx 0)
         (define smooth_normal 0)
         (define cor ground_cor)
         (define friction ground_friction)
         (define vel_scale ground_vel_scale)
         (define texture_coord_scale 0.5)
         (define a (+ (* points_per_row (+ i 1)) 0))
         (define b (+ (* points_per_row (+ i 1)) 1))
         (define c (+ (* points_per_row (+ i 0)) 1))
         (define d (+ (* points_per_row (+ i 0)) 0))
         (define p0 (vector-ref points a))
         (define p1 (vector-ref points b))
         (define p2 (vector-ref points c))
         (define p3 (vector-ref points d))
         (define atcx (+ (car p0) texture_dx))
         (define atcy (+ (caddr p0) texture_dy))
         (define btcx (+ (car p1) texture_dx))
         (define btcy (+ (caddr p1) texture_dy))
         (define ctcx (+ (car p2) texture_dx))
         (define ctcy (+ (caddr p2) texture_dy))
         (define dtcx (+ (car p3) texture_dx))
         (define dtcy (+ (caddr p3) texture_dy))
         (terrain_model_add_face num_points mat_idx smooth_normal 
                                 a b c d 
                                 atcx atcy btcx btcy ctcx ctcy dtcx dtcy texture_coord_scale
                                 cor friction vel_scale 0)

         (if (> make_border 0)
             (begin
               (set! a (+ (* points_per_row (+ i 0)) 2))
               (set! b (+ (* points_per_row (+ i 1)) 2))
               (set! c (+ (* points_per_row (+ i 1)) 3))
               (set! d (+ (* points_per_row (+ i 0)) 3))
               (terrain_model_add_face num_points 1 0 
                                       a b c d 
                                       0 0 0 0 0 0 0 0 0.25
                                       wall_cor wall_friction wall_vel_scale 1)

               (set! a (+ (* points_per_row (+ i 0)) 3))
               (set! b (+ (* points_per_row (+ i 1)) 3))
               (set! c (+ (* points_per_row (+ i 1)) 4))
               (set! d (+ (* points_per_row (+ i 0)) 4))
               (terrain_model_add_face num_points 1 0 
                                       a b c d 
                                       0 0 0 0 0 0 0 0 0.25
                                       cor friction vel_scale 3)

               (set! a (+ (* points_per_row (+ i 0)) 0))
               (set! b (+ (* points_per_row (+ i 0)) 4))
               (set! c (+ (* points_per_row (+ i 1)) 4))
               (set! d (+ (* points_per_row (+ i 1)) 0))
               (terrain_model_add_face num_points 1 0 
                                       a b c d 
                                       0 0 0 0 0 0 0 0 0.25
                                       wall_cor wall_friction wall_vel_scale 2)

               (set! a (+ (* points_per_row (+ i 0)) 1))
               (set! b (+ (* points_per_row (+ i 1)) 1))
               (set! c (+ (* points_per_row (+ i 1)) 5))
               (set! d (+ (* points_per_row (+ i 0)) 5))
               (terrain_model_add_face num_points 1 0 
                                       a b c d 
                                       0 0 0 0 0 0 0 0 0.25
                                       wall_cor wall_friction wall_vel_scale 2)

               (set! a (+ (* points_per_row (+ i 0)) 6))
               (set! b (+ (* points_per_row (+ i 0)) 5))
               (set! c (+ (* points_per_row (+ i 1)) 5))
               (set! d (+ (* points_per_row (+ i 1)) 6))
               (terrain_model_add_face num_points 1 0 
                                       a b c d 
                                       0 0 0 0 0 0 0 0 0.25
                                       cor friction vel_scale 3)

               (set! a (+ (* points_per_row (+ i 0)) 7))
               (set! b (+ (* points_per_row (+ i 0)) 6))
               (set! c (+ (* points_per_row (+ i 1)) 6))
               (set! d (+ (* points_per_row (+ i 1)) 7))
               (terrain_model_add_face num_points 1 0 
                                       a b c d 
                                       0 0 0 0 0 0 0 0 0.25
                                       wall_cor wall_friction wall_vel_scale 1)
               )
             '()
             )

         (set! dist0 dist1)
         (set! i (+ i 1))
         )

    (if (> make_back 0)
        (begin
          (define a (+ (* points_per_row N) 7))
          (define b (+ (* points_per_row N) 6))
          (define c (+ (* points_per_row N) 5))
          (define d (+ (* points_per_row N) 1))
          (terrain_model_add_face 4 1 0 
                                  a b c d 
                                  0 0 0 0 0 0 0 0 0.25
                                  wall_cor wall_friction wall_vel_scale 1)

          (set! a (+ (* points_per_row N) 7))
          (set! b (+ (* points_per_row N) 1))
          (set! c (+ (* points_per_row N) 0))
          (set! d (+ (* points_per_row N) 2))
          (terrain_model_add_face 4 1 0 
                                  a b c d 
                                  0 0 0 0 0 0 0 0 0.25
                                  wall_cor wall_friction wall_vel_scale 1)

          (set! a (+ (* points_per_row N) 2))
          (set! b (+ (* points_per_row N) 0))
          (set! c (+ (* points_per_row N) 4))
          (set! d (+ (* points_per_row N) 3))
          (terrain_model_add_face 4 1 0 
                                  a b c d 
                                  0 0 0 0 0 0 0 0 0.25
                                  wall_cor wall_friction wall_vel_scale 1)
          )
        '()
        )
  )

