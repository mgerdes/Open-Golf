(define (generate N width length texture_width texture_dx texture_dy 
                  bp0x bp0y bp1x bp1y bp2x bp2y bp3x bp3y
				  friction cor vel_scale)
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

  (define radius (* 0.5 width))
  (define cx radius)
  (define cy (bezier bp0y bp1y bp2y bp3y 0))
  (define cz radius)

  (define points_idx 0)
  (define points (make-vector (round (+ (* N N ) 4))))

  (define (add_point x y z)
    (terrain_model_add_point x y z)
    (vector-set! points points_idx (list x y z))
    (set! points_idx (+ points_idx 1))
    )

  (define (add_face num_points mat_idx smooth_normal 
                    a b c d 
                    tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y 
                    texture_coord_scale cor friction vel_scale)
    (terrain_model_add_face num_points mat_idx smooth_normal 
                            a b c d 
                            (+ tc0x texture_dx) (+ tc0y texture_dy)
                            (+ tc1x texture_dx) (+ tc1y texture_dy)
                            (+ tc2x texture_dx) (+ tc2y texture_dy)
                            (+ tc3x texture_dx) (+ tc3y texture_dy)
                            texture_coord_scale cor friction vel_scale)
    )

  (add_point cx cy cz)
  (define i 0)
  (define j 0)
  (while (< i (- N 1))
         (define t0 (/ i (- N 2)))
         (define r (+ 0.36 (* (- radius 0.36) t0)))
         (define y (bezier bp0y bp1y bp2y bp3y t0))
         (set! j 0)
         (while (< j N)
                (define t1 (/ j N))
                (define x (+ cx (* r (sin (* 2 pi t1))))) 
                (define z (+ cz (* r (cos (* 2 pi t1))))) 
                (add_point x y z)
                (set! j (+ j 1))
                )
         (set! i (+ i 1))
         )
  (add_point width 0 width)
  (add_point width 0 0)
  (add_point 0 0 0)
  (add_point 0 0 width)

  (set! i 0)
  (while (< i N)
         (define a 0)
         (define b (+ 1 i))
         (define c (+ 2 i))
         (if (= i (- N 1))
             (set! c 1)
             '())
         (define pa (vector-ref points a)) 
         (define pb (vector-ref points b)) 
         (define pc (vector-ref points c)) 

         (define num_points 3)
         (define mat_idx 0)
         (define smooth_normal 1)
         (define tc0x (car pa))
         (define tc0y (caddr pa))
         (define tc1x (car pb))
         (define tc1y (caddr pb))
         (define tc2x (car pc))
         (define tc2y (caddr pc))
         (add_face num_points mat_idx smooth_normal 
                                 a b c 0 
                                 tc0x tc0y tc1x tc1y tc2x tc2y 0 0 0.5
                                 cor friction vel_scale)
         (set! i (+ i 1))
         )

  (set! i 0)
  (while (< i (- N 2))
         (set! j 0)
         (while (< j N)
                (define a (+ 1 (* i N) j))
                (define b (+ 1 (* (+ i 1) N) j))
                (define c (+ 1 (* (+ i 1) N) j 1))
                (define d (+ 1 (* i N) j 1))
                (if (= j (- N 1))
                    (set! c (+ 1 (* (+ i 1) N)))
                    '())
                (if (= j (- N 1))
                    (set! d (+ 1 (* i N)))
                    '())

                (define pa (vector-ref points (round a)))
                (define pb (vector-ref points (round b)))
                (define pc (vector-ref points (round c)))
                (define pd (vector-ref points (round d)))

                (define num_points 4)
                (define mat_idx 0)
                (define smooth_normal 1)
                (define tc0x (car pa))
                (define tc0y (caddr pa))
                (define tc1x (car pb))
                (define tc1y (caddr pb))
                (define tc2x (car pc))
                (define tc2y (caddr pc))
                (define tc3x (car pd))
                (define tc3y (caddr pd))
                (add_face num_points mat_idx smooth_normal 
                                        a b c d 
                                        tc0x tc0y tc1x tc1y tc2x tc2y tc3x tc3y 0.5
                                        cor friction vel_scale)
                (set! j (+ j 1))
                )
         (set! i (+ i 1))
         )

  (set! i 0)
  (set! j 0)
  (while (< i N)
         (define t (/ (+ i 1) N))
         (cond 
           ((and (> t 0.25) (= j 0))

            (set! j 1)
            (define a (+ 1 (* (- N 1) N) 1))
            (define b (+ 1 (* (- N 2) N) i))
            (define c (+ 1 (* (- N 1) N) 0))
            (define pa (vector-ref points (round a)))
            (define pb (vector-ref points (round b)))
            (define pc (vector-ref points (round c)))

            (define num_points 3)
            (define mat_idx 0)
            (define smooth_normal 1)
            (define tc0x (car pa))
            (define tc0y (caddr pa))
            (define tc1x (car pb))
            (define tc1y (caddr pb))
            (define tc2x (car pc))
            (define tc2y (caddr pc))
            (add_face num_points mat_idx smooth_normal 
                                    a b c 0 
                                    tc0x tc0y tc1x tc1y tc2x tc2y 0 0 0.5
                                    cor friction vel_scale)
            )
           ((and (> t 0.5) (= j 1))

            (set! j 2)
            (define a (+ 1 (* (- N 1) N) 2))
            (define b (+ 1 (* (- N 2) N) i))
            (define c (+ 1 (* (- N 1) N) 1))
            (define pa (vector-ref points (round a)))
            (define pb (vector-ref points (round b)))
            (define pc (vector-ref points (round c)))

            (define num_points 3)
            (define mat_idx 0)
            (define smooth_normal 1)
            (define tc0x (car pa))
            (define tc0y (caddr pa))
            (define tc1x (car pb))
            (define tc1y (caddr pb))
            (define tc2x (car pc))
            (define tc2y (caddr pc))
            (add_face num_points mat_idx smooth_normal 
                                    a b c 0 
                                    tc0x tc0y tc1x tc1y tc2x tc2y 0 0 0.5
                                    cor friction vel_scale)
            )
           ((and (> t 0.75) (= j 2))

            (set! j 3)
            (define a (+ 1 (* (- N 1) N) 3))
            (define b (+ 1 (* (- N 2) N) i))
            (define c (+ 1 (* (- N 1) N) 2))
            (define pa (vector-ref points (round a)))
            (define pb (vector-ref points (round b)))
            (define pc (vector-ref points (round c)))

            (define num_points 3)
            (define mat_idx 0)
            (define smooth_normal 1)
            (define tc0x (car pa))
            (define tc0y (caddr pa))
            (define tc1x (car pb))
            (define tc1y (caddr pb))
            (define tc2x (car pc))
            (define tc2y (caddr pc))
            (add_face num_points mat_idx smooth_normal 
                                    a b c 0 
                                    tc0x tc0y tc1x tc1y tc2x tc2y 0 0 0.5
                                    cor friction vel_scale)
            )
           (else '())
           )

         (begin 
           (define a (+ 1 (* (- N 2) N) i))
           (define b (+ 1 (* (- N 1) N) j))
           (define c (+ 1 (* (- N 2) N) i 1))
           (if (= i (- N 1))
               (set! c (+ 1 (* (- N 2) N)))
               '())
           (define pa (vector-ref points (round a)))
           (define pb (vector-ref points (round b)))
           (define pc (vector-ref points (round c)))

           (define num_points 3)
           (define mat_idx 0)
           (define smooth_normal 1)
           (define tc0x (car pa))
           (define tc0y (caddr pa))
           (define tc1x (car pb))
           (define tc1y (caddr pb))
           (define tc2x (car pc))
           (define tc2y (caddr pc))
           (add_face num_points mat_idx smooth_normal 
                                   a b c 0 
                                   tc0x tc0y tc1x tc1y tc2x tc2y 0 0 0.5
                                   cor friction vel_scale)
           )

         (set! i (+ i 1))
         )

  (begin
    (define a (+ 1 (* (- N 1) N)))
    (define b (+ 1 (* (- N 2) N)))
    (define c (+ 1 (* (- N 1) N) 3))
    (define pa (vector-ref points (round a)))
    (define pb (vector-ref points (round b)))
    (define pc (vector-ref points (round c)))

    (define num_points 3)
    (define mat_idx 0)
    (define smooth_normal 1)
    (define tc0x (car pa))
    (define tc0y (caddr pa))
    (define tc1x (car pb))
    (define tc1y (caddr pb))
    (define tc2x (car pc))
    (define tc2y (caddr pc))
    (add_face num_points mat_idx smooth_normal 
                            a b c 0 
                            tc0x tc0y tc1x tc1y tc2x tc2y 0 0 0.5
                            cor friction vel_scale)
    )
  )
