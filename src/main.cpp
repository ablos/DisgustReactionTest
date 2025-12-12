#include <Arduino.h>

// !! IMPORTANT !! - This number MUST be an even number
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
bool awaitingReaction = false;
unsigned long startWaitTime = 0;
unsigned long startReactionTime = 0;

int trialSequence[TEST_COUNT]; // 0 = disgust, 1 = normal
unsigned long disgustReactionTimes[TEST_COUNT / 2];
unsigned long normalReactionTimes[TEST_COUNT / 2];
int disgustReactionCount = 0;
int normalReactionCount = 0;
int disgustWrong = 0;
int normalWrong = 0;
int disgustEarly = 0;
int normalEarly = 0;
int trialIndex = 0;
int currentTouch = 0;

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
    digitalWrite(reactionLedPin1, LOW);
    digitalWrite(reactionLedPin2, LOW);
    digitalWrite(reactionLedPin3, LOW);
    digitalWrite(reactionLedPin4, LOW);
    digitalWrite(confirmLedPin, LOW);
    
    // Calibrate touch sensors
    touchThreshold1 = touchRead(touchPin1) * thresholdPercentage;
    touchThreshold2 = touchRead(touchPin2) * thresholdPercentage;
    touchThreshold3 = touchRead(touchPin3) * thresholdPercentage;
    touchThreshold4 = touchRead(touchPin4) * thresholdPercentage;

    // Generate sequence (0 = disgust, 1 = normal)
    int halfTestCount = TEST_COUNT / 2;
    
    // Fill exactly half disgust + half normal
    for (int i = 0; i < halfTestCount; i++) 
    {
        trialSequence[i] = 0;                   // disgust
        trialSequence[i + halfTestCount] = 1;   // normal
    }
    
    // Fisher-Yates shuffle for perfect randomness
    for (int i = TEST_COUNT - 1; i > 0; i--) 
    {
        int j = random(i + 1);
        int temp = trialSequence[i];
        trialSequence[i] = trialSequence[j];
        trialSequence[j] = temp;
    }

    Serial.println("begin");
    resetReaction();
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
    
    // Get current trial
    int currentTrial = trialSequence[trialIndex];

    // When all reactions are captured, calculate average
    if (disgustReactionCount + normalReactionCount >= TEST_COUNT) 
    {
        // Calculate averages
        int normalAverage = calculateAverage(normalReactionTimes);
        int disgustAverage = calculateAverage(disgustReactionTimes);
        int totalAverage = (normalAverage + disgustAverage) / 2;

        Serial.println("done");
        
        // Print results
        Serial.print("Normal average: ");
        Serial.println(normalAverage);
        Serial.print("Disgust average: ");
        Serial.println(disgustAverage);
        Serial.print("Total average: ");
        Serial.println(totalAverage);
        Serial.println();
        Serial.print("Normal early: ");
        Serial.println(normalEarly);
        Serial.print("Disgust early: ");
        Serial.println(disgustEarly);
        Serial.println();
        Serial.print("Normal wrong: ");
        Serial.println(normalWrong);
        Serial.print("Disgust wrong: ");
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
            
            if (currentTrial == 0)
                disgustEarly++;
            else
                normalEarly++;

            resetReaction();
        }
        
        // If random interval has passed test reaction time
        else if (millis() - startWaitTime >= seconds_before_next * 1000UL) 
        {
            // Decide random touch sensor in category (touch 1 & 2 are normal - touch 3 & 4 are disgust)
            currentTouch = currentTrial == 0 ? random(2, 4) : random(2);
            digitalWrite(reactionLedPins[currentTouch], HIGH);
            awaitingReaction = true;
            Serial.println("testing");
            startReactionTime = micros();
        }
    }
    
    // Currently testing reaction time
    else 
    {
        // Check for wrong input
        if (anyTouch && !touches[currentTouch]) 
        {
            digitalWrite(confirmLedPin, HIGH);
            digitalWrite(reactionLedPins[currentTouch], LOW);
            Serial.println("wrong");

            if (currentTrial == 0)
                disgustWrong++;
            else
                normalWrong++;

            resetReaction();
            return;
        }

        // Capture reaction time on touch and reset
        if (touches[currentTouch]) 
        {
            unsigned long reactionTime = micros() - startReactionTime;
            digitalWrite(reactionLedPins[currentTouch], LOW);
            digitalWrite(confirmLedPin, HIGH);
            Serial.println("success");
            Serial.println(reactionTime);

            // 0 = disgust - 1 = normal
            if (currentTrial == 0) 
                disgustReactionTimes[disgustReactionCount++] = reactionTime;
            else
                normalReactionTimes[normalReactionCount++] = reactionTime;

            // Move to next trial
            trialIndex++;

            resetReaction();
        }
    }
}

void resetReaction() 
{
    delay(1000);
    digitalWrite(confirmLedPin, LOW);
    seconds_before_next = random(minSecondsInterval, maxSecondsInterval);
    awaitingReaction = false;
    Serial.println("reset");
    startWaitTime = millis();
}


