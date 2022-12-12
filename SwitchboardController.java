package com.thegeekylad.marcusbackend.controller;

import com.thegeekylad.marcusbackend.singletons.BrokerConnection;
import com.thegeekylad.marcusbackend.store.Actions;
import com.thegeekylad.marcusbackend.store.Tasks;
import com.thegeekylad.marcusbackend.model.SimpleResponse;
import com.thegeekylad.marcusbackend.model.power.State;
import com.thegeekylad.marcusbackend.util.Helpers;
import org.eclipse.paho.client.mqttv3.IMqttClient;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.springframework.web.bind.annotation.*;

import static com.thegeekylad.marcusbackend.util.Helpers.*;

@RestController
@RequestMapping("/toggle")
public class SwitchboardController {

    @PostMapping("/alexa")
    public synchronized SimpleResponse toggleSwitchState(@RequestBody State state) {

        Helpers.log("Received power handler ping from Alexa.", true);

        try {
            IMqttClient client = BrokerConnection.getInstance().getClient();

            // decide what command needs to be run
            String taskCommand = Tasks.TASK_STATE_CHANGE + "::" + state.getDeviceId() + ":" + state.getState();

            MqttMessage message = new MqttMessage(taskCommand.getBytes());
            message.setQos(0);
            message.setRetained(false);

            client.publish(state.getBoardTopic(), message);

            Helpers.log("Sent command to switchboard.", false);
        } catch (Exception e) {
            e.printStackTrace();
            return new SimpleResponse(Actions.POWER_STATE_CHANGE, null, e.getMessage());
        }

        return new SimpleResponse(Actions.POWER_STATE_CHANGE, null, null);
    }

    @PostMapping("/googleAssistant")
    public synchronized SimpleResponse toggleAssistantSwitchState(@RequestBody State state) {

        Helpers.log("Received power handler ping from Google Assistant.", true);

        try {
            IMqttClient client = BrokerConnection.getInstance().getClient();

            // decide what command needs to be run
            String taskCommand = Tasks.TASK_STATE_CHANGE + "::" + state.getDeviceId() + ":" + state.getState();

            MqttMessage message = new MqttMessage(taskCommand.getBytes());
            message.setQos(0);
            message.setRetained(false);

            client.publish(state.getBoardTopic(), message);

            Helpers.log("Sent command to switchboard.", false);
        } catch (Exception e) {
            e.printStackTrace();
            return new SimpleResponse(Actions.POWER_STATE_CHANGE, null, e.getMessage());
        }

        return new SimpleResponse(Actions.POWER_STATE_CHANGE, null, null);
    }

    @GetMapping("/attentionGrabber")
    @CrossOrigin("*")
    public synchronized SimpleResponse attentionGrabber() {

        Helpers.log("Received attention grabber request!", true);

        try {
            IMqttClient client = BrokerConnection.getInstance().getClient();

            for (int c = 0; c <= 3; c++) {
                // decide what command needs to be run
                String taskCommand = Tasks.TASK_STATE_CHANGE + "::" + 2 + ":" + (c % 2 == 0 ? 1 : 0);

                MqttMessage message = new MqttMessage(taskCommand.getBytes());
                message.setQos(0);
                message.setRetained(false);

                client.publish("marcus-switchboard-bedroom-a", message);

                Thread.sleep(1500);
            }

            Helpers.log("Sent command to switchboard.", false);
        } catch (Exception e) {
            e.printStackTrace();
            return new SimpleResponse(Actions.POWER_STATE_CHANGE, null, e.getMessage());
        }

        return new SimpleResponse(Actions.POWER_STATE_CHANGE, null, null);
    }
}
