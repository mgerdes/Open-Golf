(define (generate N0 N1 width height length border_width start_angle friction cor vel_scale)
  (define-macro (while test . body) 
                `(call-with-exit
                   (lambda (break) 
                     (let continue ()
                       (if (let () ,test)
                           (begin 
                             (let () ,@body)
                             (continue))
                           (break))))))

  (define end_angle (- (* 2.0 pi) start_angle))
  (define radius (* 0.5 width))
  (define outer_radius (+ radius border_width))
  (define y0 (- (* radius (sin (- start_angle (* 0.5 pi))))))

  (define i 0)
  (define j 0)
  (while (< i N0)
         (define t0 (/ i (- N0 1)))
         (set! j 0)
         (while (< j N1)
                (define t1 (/ j (- N1 1)))
                (define theta (+ (* start_angle (- 1 t1)) (* end_angle t1)))
                (define theta_actual (- theta (* 0.5 pi)))
                (define x (* radius (cos theta_actual)))
                (define y (* height (+ y0 (* radius (sin theta_actual)))))
                (define z (* length t0))
                (terrain_model_add_point x y z)

                (cond ((< theta (* 0.5 pi))
                       (set! x outer_radius)
                       )
                      ((< theta (* 1.5 pi))
                       (set! x (* outer_radius (cos theta_actual)))
                       )
                      (else
                        (set! x (- outer_radius))
                        )
                      )
                (set! y (* height (+ y0 (* outer_radius (sin theta_actual)))))
                (terrain_model_add_point x y z)

                (set! j (+ j 1))
                )

         (set! i (+ i 1))
         )

  (begin
    (define x (+ (* 0.5 width) border_width)) 
    (define y -1) 
    (define z 0) 
    (terrain_model_add_point x y z)
    )

  (begin
    (define x (+ (* 0.5 width) border_width)) 
    (define y -1) 
    (define z length) 
    (terrain_model_add_point x y z)
    )

  (begin
    (define x (- (* -0.5 width) border_width)) 
    (define y -1) 
    (define z 0) 
    (terrain_model_add_point x y z)
    )

  (begin
    (define x (- (* -0.5 width) border_width)) 
    (define y -1) 
    (define z length) 
    (terrain_model_add_point x y z)
    )

  (set! i 0)
  (while (< i (- N0 1))
         (set! j 0)
         (while (< j (- N1 1))
                (define a (+ (* (+ i 0) (* 2 N1)) (* 2 (+ j 0))))
                (define b (+ (* (+ i 1) (* 2 N1)) (* 2 (+ j 0))))
                (define c (+ (* (+ i 1) (* 2 N1)) (* 2 (+ j 1))))
                (define d (+ (* (+ i 0) (* 2 N1)) (* 2 (+ j 1))))
                (define tc0x 0.0)
                (define tc1x 0.75)
                (define tc0y (* (/ j N1) 1.5))
                (define tc1y (* (/ (+ j 1) N1) 1.5))
                (terrain_model_add_face 4 1 1
                                        a b c d
                                        tc0x tc0y 
                                        tc1x tc0y 
                                        tc1x tc1y 
                                        tc0x tc1y
                                        1.0 cor friction vel_scale 0) 

                (define a (+ (* (+ i 0) (* 2 N1)) (+ (* 2 (+ j 0)) 1)))
                (define b (+ (* (+ i 0) (* 2 N1)) (+ (* 2 (+ j 1)) 1)))
                (define c (+ (* (+ i 1) (* 2 N1)) (+ (* 2 (+ j 1)) 1)))
                (define d (+ (* (+ i 1) (* 2 N1)) (+ (* 2 (+ j 0)) 1)))
                (define tc0x 0.0)
                (define tc1x 0.75)
                (define tc0y (* (/ j N1) 1.5))
                (define tc1y (* (/ (+ j 1) N1) 1.5))
                (terrain_model_add_face 4 1 1
                                        a b c d
                                        tc1x tc0y
                                        tc1x tc1y
                                        tc0x tc1y
                                        tc0x tc0y
                                        1.0 cor friction vel_scale 0) 

                (set! j (+ j 1))
                )

         (set! i (+ i 1))
         )

  (set! j 0)
  (while (< j (- N1 1))
         (define a (+ (* 0 (* 2 N1)) (+ (* 2 (+ j 0)) 1)))
         (define b (+ (* 0 (* 2 N1)) (+ (* 2 (+ j 0)) 0)))
         (define c (+ (* 0 (* 2 N1)) (+ (* 2 (+ j 1)) 0)))
         (define d (+ (* 0 (* 2 N1)) (+ (* 2 (+ j 1)) 1)))
         (define tc0x 0.0)
         (define tc1x 0.25)
         (define tc0y (* (/ j N1) 1.5)) 
         (define tc1y (* (/ (+ j 1) N1) 1.5)) 
         (terrain_model_add_face 4 1 0
                                 a b c d
                                 tc0x tc0y
                                 tc1x tc0y
                                 tc1x tc1y
                                 tc0x tc1y
                                 1.0 cor friction vel_scale 0) 

         (set! a (+ (* 1 (* 2 N1)) (+ (* 2 (+ j 0)) 0)))
         (set! b (+ (* 1 (* 2 N1)) (+ (* 2 (+ j 0)) 1)))
         (set! c (+ (* 1 (* 2 N1)) (+ (* 2 (+ j 1)) 1)))
         (set! d (+ (* 1 (* 2 N1)) (+ (* 2 (+ j 1)) 0)))
         (define tc0x 0.0)
         (define tc1x 0.25)
         (define tc0y (* (/ j N1) 1.5)) 
         (define tc1y (* (/ (+ j 1) N1) 1.5)) 
         (terrain_model_add_face 4 1 0
                                 a b c d
                                 tc1x tc1y
                                 tc0x tc1y
                                 tc0x tc0y
                                 tc1x tc0y
                                 1.0 cor friction vel_scale 0) 

         (set! j (+ j 1))
         )

  (begin
    (define a (+ (* 2 N0 N1) 0))
    (define b (+ (* 0 N1) 1))
    (define c (+ (* 2 N1) 1))
    (define d (+ (* 2 N0 N1) 1))
    (terrain_model_add_face 4 1 1
                            a b c d
                            0 0 0 0 0 0 0 0
                            1.0 cor friction vel_scale 1) 

    )

  (begin
    (define a (+ (* 2 N0 N1) 3))
    (define b (+ (* 4 N1) -1))
    (define c (+ (* 2 N1) -1))
    (define d (+ (* 2 N0 N1) 2))
    (terrain_model_add_face 4 1 1
                            a b c d
                            0 0 0 0 0 0 0 0
                            1.0 cor friction vel_scale 1) 

    )
  )
