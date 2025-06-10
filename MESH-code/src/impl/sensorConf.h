//
// Created by vm on 25.10.6.
//

#ifndef USE

bool _() {
#endif

    NODE(0x3133)
    {
        CH(1) {
            return hw_read_analog(adc, 0);
        }
    }
    NODE(0x3f50, 0x2850)
    {
        CH(1) {
            ld2410b_processReport(&ld);
            return ld.target_distanceStationary;
        }
    }

#ifndef USE
}

#endif
