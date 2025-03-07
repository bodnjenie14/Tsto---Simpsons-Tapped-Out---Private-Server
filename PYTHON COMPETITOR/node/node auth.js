import { Router } from "express";

import sqlite3 from "sqlite3";

import config from "../../../../config.json" with { type: "json" };

import jwt from "jsonwebtoken";
import generateToken from "./tokenGen.js";

const db = new sqlite3.Database(
  config.dataDirectory + "/users.db",
  sqlite3.OPEN_READWRITE | sqlite3.OPEN_CREATE,
  (error) => {
    if (error) {
      console.error("Error opening database:", error.message);
    }
  },
);

const router = Router();

router.get("/auth", async (req, res, next) => {
  try {
    const response_type = req.query.response_type.split(" "); // For example "code lngvl_token" or "code"

    if (req.query.authenticator_login_type == "mobile_anonymous") {
      // Generate new account
      const LAST_USER_QUERY =
        "SELECT MayhemId, UserId from UserData ORDER BY UserId DESC LIMIT 1;";
      await db.get(LAST_USER_QUERY, async (error, row) => {
        // Get the last created user, so we can make the new users ids +1
        if (error) {
          console.error("Error executing query:", error.message);
          res.status(500).send({ message: "Internal error" });
          return;
        }

        const newUID = row
          ? Number(row.UserId) + 1
          : config.startingUID + 1 /* If there are no users */;
        const newMID = row
          ? Number(row.MayhemId) + 1
          : config.startingMID + 1 /* If there are no users */;

        const newAccessToken = generateToken("AT", newUID.toString());
        const newAccessCode = generateToken("AC", newUID.toString());

        const NEW_USER_QUERY = `INSERT INTO UserData (UserId, MayhemId, UserAccessToken, UserAccessCode) VALUES (${newUID}, ${newMID}, \"${newAccessToken}\", \"${newAccessCode}\");`;
        await db.run(NEW_USER_QUERY);

        const response = {};
        if (response_type.includes("code")) {
          response.code = newAccessCode;
        }
        if (response_type.includes("lngvl_token")) {
          response.lngvl_token = newAccessToken;
        }

        res.status(200).send(response);
      });
    } else if (req.query.authenticator_login_type == "mobile_ea_account") {
      // EA
    } else {
      res.status(400).send({ message: "Unknown authenticator_login_type" });
      return;
    }

    /*let accessCode = generateToken("AC", "1017814658519");
    res.status(200).send({ "code": accessCode });*/
  } catch (error) {
    next(error);
  }
});

router.post("/token", async (req, res, next) => {
  try {
    // Add actual reading of db
    const code = req.query.code;

    const USER_BY_CODE_QUERY =
      "SELECT UserId, UserAccessToken FROM UserData WHERE UserAccessCode = ?;";
    await db.get(USER_BY_CODE_QUERY, [code], async (error, row) => {
      if (!row) {
        res
          .status(400)
          .send({ message: "No user could be found with that UserAccessCode" });
        return;
      }

      res.status(200).send({
        access_token: row.UserAccessToken,
        expires_in: 4242, // Never expires (for now)
        id_token: jwt.sign(
          {
            aud: "simpsons4-android-client",
            iss: "accounts.ea.com",
            iat: Math.floor(Date.now() / 1000),
            exp: Math.floor(Date.now() / 1000) + 4242,
            pid_id: row.UserId.toString(), // All the same, so it's easier to handle
            user_id: row.UserId.toString(),
            persona_id: row.UserId,
            pid_type: "AUTHENTICATOR_ANONYMOUS", // Probably not important, so we should be able to use it for mobile_ea_account too
            auth_time: 0,
          },
          "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5", // Thank you tehfens
        ),
        refresh_token: "NotImplemented", // Not Implemented Yet
        refresh_token_expires_in: 4242, // Not Implemented Yet
        token_type: "Bearer",
      });
    });
  } catch (error) {
    next(error);
  }
});

router.get("/tokeninfo", async (req, res, next) => {
  try {
    // Header for include authenticator etc
    const accessToken = req.headers["access_token"] || req.query.access_token;

    const USER_BY_TOKEN_QUERY =
      "SELECT UserId FROM UserData WHERE UserAccessToken = ?;";
    await db.get(USER_BY_TOKEN_QUERY, [accessToken], async (error, row) => {
      if (!row) {
        res
          .status(400)
          .send({
            message: "No user could be found with that UserAccessToken",
          });
        return;
      }

      let response = {
        client_id: "simpsons4-android-client", // Always the same
        expires_in: 4242,
        persona_id: row.UserId,
        pid_id: row.UserId.toString(),
        pid_type: "AUTHENTICATOR_ANONYMOUS",
        scope:
          "offline basic.antelope.links.bulk openid signin antelope-rtm-readwrite search.identity basic.antelope basic.identity basic.persona antelope-inbox-readwrite", // Always this for anonymous accounts
        user_id: row.UserId.toString(),
      };
      if (req.headers["x-check-underage"] == "true") {
        response.is_underage = null;
      }
      if (req.headers["x-include-authenticators"] == "true") {
        response.authenticators = [
          {
            authenticator_pid_id: row.UserId,
            authenticator_type: "AUTHENTICATOR_ANONYMOUS",
          }, // There is an additional one for ea accounts
        ];
      }
      if (req.headers["x-include-stopprocess"] == "true") {
        response.stopProcess = "OFF";
      }
      if (req.headers["x-include-tid"] == "true") {
        response.telemetry_id = row.UserId;
      }

      res.status(200).send(response);
    });
  } catch (error) {
    next(error);
  }
});

export default router;