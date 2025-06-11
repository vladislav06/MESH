//
// Created by vm on 25.10.6.
//

#ifndef USE

bool _() {
#endif

    NODE(0x3133,0x2f33)
    {
        CH(1) {
            return hw_read_analog(adc, 0);
        }
    }
    NODE(0x3f50, 0x2850,0x3f4d)
    {
        CH(1) {
            ld2410b_processReport(&ld);
            return ld.target_distanceStationary;
        }
    }

#ifndef USE
}

#endif
