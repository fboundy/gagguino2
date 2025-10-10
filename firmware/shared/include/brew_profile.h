#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**\brief Modes that describe how a brew phase's duration is evaluated. */
typedef enum {
    BREW_DURATION_TIME,   //!< Duration measured in seconds
    BREW_DURATION_VOLUME, //!< Duration measured by dispensed volume (mL)
    BREW_DURATION_MASS    //!< Duration measured by mass (g)
} BrewDurationMode;

/**\brief Modes describing how pump control values are interpreted. */
typedef enum {
    BREW_PUMP_POWER,    //!< Pump output is a percent-based duty cycle
    BREW_PUMP_PRESSURE  //!< Pump output targets a pressure in bar
} BrewPumpMode;

/**\brief Definition of a single brew phase within a profile. */
typedef struct {
    const char *name;            //!< Identifier for the phase (max 128 characters)
    BrewDurationMode durationMode; //!< How to interpret the durationValue
    uint32_t durationValue;        //!< Duration in seconds, millilitres, or grams
    BrewPumpMode pumpMode;         //!< Pump control mode for the phase
    float pumpValue;               //!< Pump power (%) or pressure (bar)
    float temperatureC;            //!< Target temperature in °C
} BrewPhase;

/**\brief Collection of brew phases that make up a brew profile. */
typedef struct {
    const char *name;            //!< Identifier for the brew profile (max 128 characters)
    const char *description;     //!< Human-readable description of the profile (optional)
    const BrewPhase *phases;     //!< Ordered phases in this profile
    size_t phaseCount;           //!< Number of phases in the profile
} BrewProfile;

/**\brief Default brew profile definition. */
static const BrewPhase BREW_PROFILE_DEFAULT_PHASES[] = {
    {
        .name = "Default",
        .durationMode = BREW_DURATION_TIME,
        .durationValue = 3600U,
        .pumpMode = BREW_PUMP_POWER,
        .pumpValue = 95.0f,
        .temperatureC = 92.0f,
    },
};

static const BrewProfile BREW_PROFILE_DEFAULT = {
    .name = "Default",
    .description = "",
    .phases = BREW_PROFILE_DEFAULT_PHASES,
    .phaseCount = sizeof(BREW_PROFILE_DEFAULT_PHASES) / sizeof(BREW_PROFILE_DEFAULT_PHASES[0]),
};

#ifdef __cplusplus
}
#endif

