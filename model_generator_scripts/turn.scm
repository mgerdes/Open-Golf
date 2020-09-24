(define (generate N inner_radius width length 
                  ground_cor ground_friction ground_vel_scale
                  border_cor border_friction border_vel_scale
                  border_height border_width border_ground_y)
  (define-macro (while test . body) 
                `(call-with-exit
                   (lambda (break) 
                     (let continue ()
                       (if (let () ,test)
                           (begin 
                             (let () ,@body)
                             (continue))
                           (break))))))

  (define points (make-vector (round (* 8 N))))
  (define points_idx 0)

  (define (add_point x y z)
    (terrain_model_add_point x y z)
    (vector-set! points points_idx (list x y z))
    (set! points_idx (+ points_idx 1))
    )

  (define i 0)
  (while (< i N)
	 (define theta (* length pi (/ i (- N 1))))

     ;; Inner Border
     (begin 
       (define x (* (- inner_radius border_width) (cos theta)))
       (define y border_ground_y)
       (define z (* (- inner_radius border_width) (sin theta)))
       (add_point x y z))

     (begin 
       (define x (* (- inner_radius border_width) (cos theta)))
       (define y border_height)
       (define z (* (- inner_radius border_width) (sin theta)))
       (add_point x y z))

     (begin 
       (define x (* inner_radius (cos theta)))
       (define y border_height)
       (define z (* inner_radius (sin theta)))
       (add_point x y z))

     ;; Inner Border
     (begin
       (define x (* inner_radius (cos theta)))
       (define y 0)
       (define z (* inner_radius (sin theta)))
       (add_point x y z))

     (begin
       (define x (* (+ inner_radius width) (cos theta)))
       (define y 0)
       (define z (* (+ inner_radius width) (sin theta)))
       (add_point x y z))

     ;; Outer Border
     (begin 
       (define x (* (+ inner_radius width) (cos theta)))
       (define y border_height)
       (define z (* (+ inner_radius width) (sin theta)))
       (add_point x y z))

     (begin 
       (define x (* (+ inner_radius width border_width) (cos theta)))
       (define y border_height)
       (define z (* (+ inner_radius width border_width) (sin theta)))
       (add_point x y z))

     (begin 
       (define x (* (+ inner_radius width border_width) (cos theta)))
       (define y border_ground_y)
       (define z (* (+ inner_radius width border_width) (sin theta)))
       (add_point x y z))

	 (set! i (+ 1 i))
	 )

  (set! i 0)
  (while (< i (- N 1))
         ;; Inner Border
         (define inner_length (* length pi inner_radius))

         (begin 
           (define a (round (+ (* 8 i) 0)))
           (define b (round (+ (* 8 i) 8)))
           (define c (round (+ (* 8 i) 9)))
           (define d (round (+ (* 8 i) 1)))

           (define tc0x 0.0)
           (define tc1x (+ border_height (- border_ground_y)))
           (define tc0y (* (/ i N) inner_length))
           (define tc1y (* (/ (+ i 1) N) inner_length))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25
                                   border_cor border_friction border_vel_scale 0))

         (begin 
           (define a (round (+ (* 8 i) 1)))
           (define b (round (+ (* 8 i) 9)))
           (define c (round (+ (* 8 i) 10)))
           (define d (round (+ (* 8 i) 2)))

           (define tc0x 0.0)
           (define tc1x 1.0)
           (define tc0y (* (/ i N) inner_length))
           (define tc1y (* (/ (+ i 1) N) inner_length))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25
                                   ground_cor ground_friction ground_vel_scale 0))

         (begin 
           (define a (round (+ (* 8 i) 3)))
           (define b (round (+ (* 8 i) 2)))
           (define c (round (+ (* 8 i) 10)))
           (define d (round (+ (* 8 i) 11)))

           (define tc0x 0.0)
           (define tc1x 1.0)
           (define tc0y (* (/ i N) inner_length))
           (define tc1y (* (/ (+ i 1) N) inner_length))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc0x tc0y
                                   tc0x tc1y
                                   tc1x tc1y
                                   0.25
                                   border_cor border_friction border_vel_scale 0))

         ;; Floor
         (begin 
           (define a (round (+ (* 8 i) 3)))
           (define b (round (+ (* 8 i) 11)))
           (define c (round (+ (* 8 i) 12)))
           (define d (round (+ (* 8 i) 4)))

           (define atcx 0)
           (define atcy 0)
           (define btcx 0)
           (define btcy 0)
           (define ctcx 0)
           (define ctcy 0)
           (define dtcx 0)
           (define dtcy 0)

           (terrain_model_add_face 4 0 0
                                   a b c d
                                   atcx atcy
                                   btcx btcy
                                   ctcx ctcy
                                   dtcx dtcy
                                   0.5
                                   ground_cor ground_friction ground_vel_scale 0))

         ;; Outer wall
         (define outer_length (* length pi (+ inner_radius width)))

         (begin 
           (define a (round (+ (* 8 i) 4)))
           (define b (round (+ (* 8 i) 12)))
           (define c (round (+ (* 8 i) 13)))
           (define d (round (+ (* 8 i) 5)))

           (define tc0x 0.0)
           (define tc1x 1.0)
           (define tc0y (* (/ i N) outer_length))
           (define tc1y (* (/ (+ i 1) N) outer_length))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25
                                   border_cor border_friction border_vel_scale 0))

         (begin 
           (define a (round (+ (* 8 i) 5)))
           (define b (round (+ (* 8 i) 13)))
           (define c (round (+ (* 8 i) 14)))
           (define d (round (+ (* 8 i) 6)))

           (define tc0x 0.0)
           (define tc1x 1.0)
           (define tc0y (* (/ i N) outer_length))
           (define tc1y (* (/ (+ i 1) N) outer_length))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc1x tc1y
                                   tc0x tc1y
                                   tc0x tc0y
                                   0.25
                                   ground_cor ground_friction ground_vel_scale 0))

         (begin 
           (define a (round (+ (* 8 i) 7)))
           (define b (round (+ (* 8 i) 6)))
           (define c (round (+ (* 8 i) 14)))
           (define d (round (+ (* 8 i) 15)))

           (define tc0x 0.0)
           (define tc1x (+ border_height (- border_ground_y)))
           (define tc0y (* (/ i N) outer_length))
           (define tc1y (* (/ (+ i 1) N) outer_length))

           (terrain_model_add_face 4 1 0
                                   a b c d
                                   tc1x tc0y
                                   tc0x tc0y
                                   tc0x tc1y
                                   tc1x tc1y
                                   0.25
                                   border_cor border_friction border_vel_scale 0))

         (set! i (+ i 1))
         )

  )


