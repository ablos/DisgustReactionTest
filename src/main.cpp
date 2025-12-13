#include <Arduino.h>

// !! IMPORTANT !! - Test count MUST be a multiple of 4!
#define TEST_COUNT 12
#define minSecondsInterval 3
#define maxSecondsInterval 7

// Touch 1 & 2 are normal
// Touch 3 & 4 are disgust
#define touchPin1 4
#define reactionLedPin1 16
#define touchPin2 13
#define reactionLedPin2 17
#define touchPin3 27
#define reactionLedPin3 18
#define touchPin4 32
#define reactionLedPin4 19

#define confirmLedPin 2
#define thresholdPercentage 0.75

int seconds_before_next = 0;
bool started = false;
bool awaitingReaction = false;
unsigned long startWaitTime = 0;
unsigned long startReactionTime = 0;

int trialSequence[TEST_COUNT]; // 0 + 1 are normal - 2 + 3 are disgust
unsigned long disgustReactionTimes[TEST_COUNT / 2];
unsigned long normalReactionTimes[TEST_COUNT / 2];
int disgustReactionCount = 0;
int normalReactionCount = 0;
int disgustWrong = 0;
int normalWrong = 0;
int disgustEarly = 0;
int normalEarly = 0;
int trialIndex = 0;

int touchThreshold1 = 75;
int touchThreshold2 = 75;
int touchThreshold3 = 75;
int touchThreshold4 = 75;

int touchPins[4] = { touchPin1, touchPin2, touchPin3, touchPin4 };
int reactionLedPins[4] = { reactionLedPin1, reactionLedPin2, reactionLedPin3, reactionLedPin4 };

void resetReaction();

template<typename T, size_t N>
unsigned long calculateAverage(T (&values)[N]) 
{
    unsigned long sum = 0;
    for (size_t i = 0; i < N; i++) sum += values[i];
    return sum / N;
}

void setup() 
{
    Serial.begin(115200);
    randomSeed(micros() ^ touchRead(touchPin1) ^ touchRead(touchPin2) ^ touchRead(touchPin3) ^ touchRead(touchPin4));
    
    // Initialize pins
    pinMode(reactionLedPin1, OUTPUT);
    pinMode(reactionLedPin2, OUTPUT);
    pinMode(reactionLedPin3, OUTPUT);
    pinMode(reactionLedPin4, OUTPUT);
    pinMode(confirmLedPin, OUTPUT);
    
    // Set initial states
    digitalWrite(reactionLedPin1, HIGH);
    digitalWrite(reactionLedPin2, HIGH);
    digitalWrite(reactionLedPin3, HIGH);
    digitalWrite(reactionLedPin4, HIGH);
    digitalWrite(confirmLedPin, LOW);
    
    // Calibrate touch sensors
    touchThreshold1 = touchRead(touchPin1) * thresholdPercentage;
    touchThreshold2 = touchRead(touchPin2) * thresholdPercentage;
    touchThreshold3 = touchRead(touchPin3) * thresholdPercentage;
    touchThreshold4 = touchRead(touchPin4) * thresholdPercentage;

    // Trail each sensor the same amount of times
    for (int i = 0; i < TEST_COUNT / 4; i++)
    {
        trialSequence[i * 4 + 0] = 0;      // sensor 1
        trialSequence[i * 4 + 1] = 1;      // sensor 2  
        trialSequence[i * 4 + 2] = 2;      // sensor 3
        trialSequence[i * 4 + 3] = 3;      // sensor 4
    }
    
    // Fisher-Yates shuffle for perfect randomness
    for (int i = TEST_COUNT - 1; i > 0; i--) 
    {
        int j = random(i + 1);
        int temp = trialSequence[i];
        trialSequence[i] = trialSequence[j];
        trialSequence[j] = temp;
    }

    Serial.print("ready,");
    Serial.println(TEST_COUNT);
}

void loop() 
{
    // Read touch values
    int touchValue1 = touchRead(touchPin1);
    int touchValue2 = touchRead(touchPin2);
    int touchValue3 = touchRead(touchPin3);
    int touchValue4 = touchRead(touchPin4);
    
    // Determine if there is a touch
    bool touch1 = touchValue1 < touchThreshold1;
    bool touch2 = touchValue2 < touchThreshold2;
    bool touch3 = touchValue3 < touchThreshold3;
    bool touch4 = touchValue4 < touchThreshold4;
    bool anyTouch = touch1 || touch2 || touch3 || touch4;
    bool touches[4] = { touch1, touch2, touch3, touch4 };
    
    // Wait before any touch if not started
    if (!started) 
    {
        if (anyTouch) 
        {
            started = true;
            Serial.println("start");
            digitalWrite(reactionLedPin1, LOW);
            digitalWrite(reactionLedPin2, LOW);
            digitalWrite(reactionLedPin3, LOW);
            digitalWrite(reactionLedPin4, LOW);
            delay(3000);
            resetReaction();
        }

        return;
    }

    // Get current trial
    int currentTrial = trialSequence[trialIndex];

    // When all reactions are captured, calculate average
    if (disgustReactionCount + normalReactionCount >= TEST_COUNT) 
    {
        // Calculate averages
        unsigned long normalAverage = calculateAverage(normalReactionTimes);
        unsigned long disgustAverage = calculateAverage(disgustReactionTimes);
        unsigned long totalAverage = (normalAverage + disgustAverage) / 2;

        Serial.print("end,");
        
        // Print results
        Serial.print(normalAverage);
        Serial.print(",");
        Serial.print(disgustAverage);
        Serial.print(",");
        Serial.print(totalAverage);
        Serial.print(",");
        Serial.print(normalEarly);
        Serial.print(",");
        Serial.print(disgustEarly);
        Serial.print(",");
        Serial.print(normalWrong);
        Serial.print(",");
        Serial.println(disgustWrong);

        // Turn of ESP -- Restart test by pressing the reset button (EN)
        esp_deep_sleep_start();
    }

    // Executing interval between tests (also monitor for early inputs to reset test)
    if (!awaitingReaction) 
    {
        // If pressed before interval time has passed cancel test and start a new one
        if (anyTouch) 
        {
            digitalWrite(confirmLedPin, HIGH);
            Serial.println("early");
            
            if (currentTrial <= 1)
                normalEarly++;
            else
                disgustEarly++;

            resetReaction();
        }
        
        // If random interval has passed test reaction time start trial
        else if (millis() - startWaitTime >= seconds_before_next * 1000UL) 
        {
            digitalWrite(reactionLedPins[currentTrial], HIGH);
            awaitingReaction = true;
            startReactionTime = micros();
        }
    }
    
    // Currently testing reaction time
    else if (anyTouch)
    {
        // Capture reaction time
        unsigned long reactionTime = micros() - startReactionTime;
        digitalWrite(reactionLedPins[currentTrial], LOW);
        digitalWrite(confirmLedPin, HIGH);
        
        Serial.print("test,");
        Serial.print(trialIndex + 1);
        Serial.print(currentTrial <= 1 ? ",normal," : ",disgust,");

        // Correct touch
        if (touches[currentTrial]) 
        {
            Serial.print("success,");

            // 0 + 1 are normal - 2 + 3 are disgust
            if (currentTrial <= 1) 
                normalReactionTimes[normalReactionCount++] = reactionTime;
            else
                disgustReactionTimes[disgustReactionCount++] = reactionTime;

            // Move to next trial
            trialIndex++;
        }
        
        // Wrong touch
        else
        {
            Serial.print("wrong,");

            if (currentTrial <= 1)
                normalWrong++;
            else
                disgustWrong++;
                
            // Reshuffle remaining trials
            int remaining = TEST_COUNT - trialIndex;
            if (remaining > 1)
            {
                for (int i = trialIndex; i < TEST_COUNT - 1; i++) 
                {
                    int j = random(i, TEST_COUNT);
                    int temp = trialSequence[i];
                    trialSequence[i] = trialSequence[j];
                    trialSequence[j] = temp;
                }
            }
        }

        Serial.println(reactionTime);
        resetReaction();
    }
}

void resetReaction() 
{
    delay(1000);
    digitalWrite(confirmLedPin, LOW);
    seconds_before_next = random(minSecondsInterval, maxSecondsInterval);
    awaitingReaction = false;
    startWaitTime = millis();
}


